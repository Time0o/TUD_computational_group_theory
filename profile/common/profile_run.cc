#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "profile_run.h"
#include "profile_timer.h"
#include "util.h"

namespace
{

class TmpFile
{
public:
  TmpFile(std::string const &content)
  {
    if (mkstemp(name) == -1)
      throw std::runtime_error("failed to create temporary file");

    _f = std::ofstream(name);

    if (_f.fail())
      throw std::runtime_error("failed to create temporary file");

    _f << content;
    _f.flush();
  }

  ~TmpFile()
  { std::remove(name); }

  char name[6] = {'X', 'X', 'X', 'X', 'X', 'X'};

private:
  std::ofstream _f;
};

std::string build_script(
  std::initializer_list<std::string> packages,
  std::initializer_list<std::tuple<std::string, std::string, bool>> preloads,
  std::string const &script,
  unsigned num_discarded_runs,
  unsigned num_runs)
{
  std::stringstream ss;

  for (auto const &package : packages)
    ss << "LoadPackage(\"" << package << "\");\n";

  for (auto const &preload : preloads)
    ss << "Read(\"" << std::get<0>(preload) << "\");\n";

  ss << "_ts:=[];\n";
  ss << "for _r in [1.." << num_discarded_runs + num_runs << "] do\n";
  ss << "  _start:=NanosecondsSinceEpoch();\n";
  ss << script;
  ss << "  if _r > " << num_discarded_runs << " then\n";
  ss << "    Add(_ts, NanosecondsSinceEpoch() - _start);\n";
  ss << "  fi;\n";
  ss << "od;\n";
  ss << "Print(\"RESULT: \", _ts, \"\\n\");\n";
  ss << "Print(\"END\\n\");\n";

  return ss.str();
}

void dup_fd(int from, int to)
{
  for (;;) {
    if (dup2(from, to) == -1) {
      if (errno == EINTR)
        continue;
      else
        throw std::runtime_error("dup failed");
    } else {
      break;
    }
  }
}

void connect_stream(int stream, int *pipe)
{
  dup_fd(pipe[1], stream);
  close(pipe[1]);
  close(pipe[0]);
}

void redirect_stream(int stream, int to)
{ dup_fd(to, stream); }

std::string read_output(int from, bool echo)
{
  static char buf[256];

  std::string res;

  for (;;) {
    auto count = read(from, buf, sizeof(buf));

    if (count == -1) {
      if (errno == EINTR)
        continue;
      else
        throw std::runtime_error("read failed");
      break;
    } else if (count == 0) {
      break;
    } else {
      std::string block(buf, buf + count);

      if (block.find("END") != std::string::npos) {
        res += block.substr(0u, block.size() - std::strlen("END") - 1u);
        break;
      }

      res += block;

      if (block.find("RESULT") != std::string::npos)
        echo = false;

      if (echo) {
        auto echo_block(block);

        echo_block.erase(std::remove(echo_block.begin(), echo_block.end(), ';'),
                         echo_block.end());

        std::cout << echo_block << std::flush;
      }
    }
  }

  return res;
}

template<typename FUNC>
std::string run_in_child(FUNC &&f,
                         int *output_pipe = nullptr,
                         bool hide_stdout = false,
                         bool hide_stderr = false)
{
  // fork child process
  pid_t child;
  switch ((child = fork())) {
  case -1:
    throw std::runtime_error("failed to fork child process");
  case 0:
    {
      // connect stdout/stderr pipes
      if (output_pipe)
        connect_stream(STDOUT_FILENO, output_pipe);

      // hide stderr
      if (hide_stderr) {
        int dev_null = open("/dev/null", O_WRONLY);

        if (dev_null != -1)
          redirect_stream(STDERR_FILENO, dev_null);

        close(dev_null);
      }

      // run function in child process
      if (f() == -1)
        _Exit(EXIT_FAILURE);
    }
  }

  // read output
  std::string output;

  if (output_pipe) {
    output = read_output(output_pipe[0], !hide_stdout);

    if (!hide_stdout)
      std::cout << std::endl;

    close(output_pipe[1]);
    close(output_pipe[0]);
  } else {
    output = "";
  }

  // check child process exit status
  int status;
  if (waitpid(child, &status, 0) == -1)
    throw std::runtime_error("waitpid failed");

  if (!WIFEXITED(status))
    throw std::runtime_error("child process did not terminate normally");

  // return output
  return output;
}

std::string clean_output(std::string const &output)
{
  std::size_t i = 0u, j = output.size() - 1u;

  while (std::isspace(output[i]))
    ++i;

  while (std::isspace(output[j]))
    --j;

  return output.substr(i, j - i + 1u);
}

std::string compress_output(std::string const &output)
{
  auto res(output);

  for (char space : " \n\\")
    res.erase(std::remove(res.begin(), res.end(), space), res.end());

  return res;
}

std::vector<std::string> parse_output(std::string const &output_,
                                      unsigned num_runs,
                                      std::vector<double> *ts)
{
  using profile::split;
  using profile::stof;

  auto output(compress_output(clean_output(output_)));

  // parse output
  auto output_split(split(output, ";"));

  std::vector<std::string> output_vals(
    output_split.begin(),
    output_split.begin() + (output_split.size() - 1u) / num_runs);

  if (ts) {
    auto parse_output_times = [](std::string output_times){
      output_times = output_times.substr(std::strlen("RESULT:"));
      output_times = output_times.substr(1u, output_times.size() - 2u);

      return split(output_times, ",");
    };

    for (auto const &output_time : parse_output_times(output_split.back()))
      ts->push_back(stof<double>(output_time) / 1e9);
  }

  return output_vals;
}

}

namespace profile
{

std::vector<std::string> run_gap(
  std::initializer_list<std::string> packages,
  std::initializer_list<std::tuple<std::string, std::string, bool>> preloads,
  std::string const &script,
  unsigned num_discarded_runs,
  unsigned num_runs,
  bool hide_output,
  bool hide_errors,
  bool compile,
  std::vector<double> *ts)
{
  // create preload files

  for (auto const &preload : preloads)
    std::ofstream(std::get<0>(preload)) << std::get<1>(preload);

  // create temporary gap script

  TmpFile f(build_script(packages, preloads, script, num_discarded_runs, num_runs));

  (void) compile; // TODO

  // create pipe for capturing gaps output

  int output_pipe[2];
  if (pipe(output_pipe) == -1)
    throw std::runtime_error("failed to create pipe");

  // run gap in a child process

  auto output(run_in_child([&]{
      return execlp("gap", "gap", "--nointeract", "-q", f.name, nullptr);
    },
    output_pipe,
    hide_output,
    hide_errors));

  // delete preloads files

  for (auto const &preload : preloads)
    std::remove(std::get<0>(preload).c_str());

  // parse and return output

  return parse_output(output, num_runs, ts);
}

void dump_runs(std::vector<double> const &ts, bool summarize)
{
  if (summarize) {
    double t_mean, t_stddev;
    util::mean_stddev(ts, &t_mean, &t_stddev);

    result("Mean:", t_mean, "s");
    result("Stddev:", t_stddev, "s");

  } else {
    result("Runtimes:");
    for (double t : ts)
      result(t, "s");
  }
}

} // namespace profile
