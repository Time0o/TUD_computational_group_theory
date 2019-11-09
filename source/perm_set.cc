#include <unordered_set>
#include <vector>
#include <queue>

#include "perm.h"
#include "perm_set.h"

namespace cgtl
{

void PermSet::make_unique()
{
  std::unordered_set<Perm> tmp(_perms.begin(), _perms.end());
  _perms = std::vector<Perm>(tmp.begin(), tmp.end());
}

void PermSet::minimize_degree()
{
  if (empty())
    return;

  std::vector<std::vector<unsigned>> moved_sets(size());

  std::vector<unsigned> compression_mapping(degree());
  for (unsigned i = 1u; i <= degree(); ++i)
    compression_mapping[i - 1u] = i;

  std::queue<unsigned> non_moved_queue;

  unsigned new_degree = 1u;

  for (unsigned i = 1u; i <= degree(); ++i) {
    bool moved = false;
    for (auto j = 0u; j < _perms.size(); ++j) {
      if (_perms[j][i] != i) {
        moved_sets[j].push_back(i);
        moved = true;
      }
    }

    if (moved) {
      if (!non_moved_queue.empty()) {
        unsigned compress_to = non_moved_queue.front();
        compression_mapping[i - 1u] = compress_to;
        new_degree = compress_to;

        non_moved_queue.pop();
        non_moved_queue.push(i);
      } else {
        new_degree = i;
      }
    } else {
      non_moved_queue.push(i);
    }
  }

  std::vector<unsigned> id(new_degree);
  for (unsigned i = 1u; i <= new_degree; ++i)
    id[i - 1u] = i;

  for (unsigned i = 0u; i < _perms.size(); ++i) {
    auto gen(id);
    for (unsigned j = 0u; j < moved_sets[i].size(); ++j) {
      unsigned x = moved_sets[i][j];
      unsigned y = _perms[i][x];

      gen[compression_mapping[x - 1u] - 1u] = compression_mapping[y - 1u];
    }

    _perms[i] = Perm(gen);
  }
}

} // namespace cgtl