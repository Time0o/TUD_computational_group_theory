#ifndef _GUARD_BSGS_H
#define _GUARD_BSGS_H

#include <cassert>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "perm.h"
#include "perm_set.h"
#include "schreier_generator_queue.h"
#include "schreier_structure.h"

namespace cgtl
{

class BSGS
{
friend std::ostream& operator<<(std::ostream& stream, BSGS const &bsgs);

  using ss_type = std::shared_ptr<SchreierStructure>;

public:
  struct SolveError : public std::runtime_error
  {
    SolveError()
    : std::runtime_error("failed to solve BSGS")
    {}
  };

  enum Construction {
    CONSTRUCTION_SCHREIER_SIMS,
    CONSTRUCTION_SCHREIER_SIMS_RANDOM,
    CONSTRUCTION_SOLVE,
    CONSTRUCTION_AUTO
  };

  enum Transversals {
    TRANSVERSALS_EXPLICIT,
    TRANSVERSALS_SCHREIER_TREES,
    TRANSVERSALS_SHALLOW_SCHREIER_TREES,
    TRANSVERSALS_AUTO
  };

  BSGS(unsigned degree,
       PermSet const &generators,
       Construction construction = CONSTRUCTION_AUTO,
       Transversals transversals = TRANSVERSALS_AUTO);

  unsigned degree() const { return _degree; }

  bool base_empty() const { return _base.empty(); }
  unsigned base_size() const { return _base.size(); }
  unsigned base_point(unsigned i) const {
    assert(i < _base.size());
    return _base[i];
  }

  // TODO: remove?
  PermSet strong_generators() const { return _strong_generators; }

  std::vector<unsigned> orbit(unsigned i) const;
  Perm transversal(unsigned i, unsigned o) const;
  PermSet transversals(unsigned i) const;
  PermSet stabilizers(unsigned i) const;

  std::pair<Perm, unsigned> strip(Perm const &perm, unsigned offs = 0) const;
  bool strips_completely(Perm const &perm) const;

  void remove_generators();

private:
  // schreier sims initialization
  void schreier_sims(PermSet const &generators);
  void schreier_sims_random(PermSet const &generators, unsigned w = 10);

  void schreier_sims_init(
    std::vector<PermSet> *strong_generators,
    std::vector<std::vector<unsigned>> *fundamental_orbits,
    std::vector<SchreierGeneratorQueue> *schreier_generator_queues = nullptr);

  void schreier_sims_finish();

  // solvable BSGS initialization
  void solve(PermSet const &generators);

  bool solve_s_normal_closure(PermSet const &generators,
                              Perm const &w,
                              std::pair<Perm, Perm> *conjugates);

  void solve_adjoin_normalizing_generator(Perm const &gen);

  // convenience methods
  void extend_base(unsigned bp);
  ss_type make_schreier_structure(unsigned bp);
  void update_schreier_structure(unsigned i, PermSet const &strong_generators);

  unsigned _degree;
  Transversals _transversals;
  std::vector<unsigned> _base;
  std::vector<ss_type> _schreier_structures;
  PermSet _strong_generators;
};

std::ostream& operator<<(std::ostream& stream, BSGS const &bsgs);

} // namespace cgtl

#endif // _GUARD_BSGS_H
