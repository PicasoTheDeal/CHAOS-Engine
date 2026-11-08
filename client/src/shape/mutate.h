#pragma once
#include "shape/parametric.h"
#include "seed/rng.h"
#include <string>
#include <vector>

namespace chaos {

// the mutation lineage: generation 0 is the classic triple (twist, maybe warp, ripple), then 1 to 4 more
// generations picked at random, each a little stronger. works in place on the positions. returns the names.
std::vector<std::string> mutateLineage(Mesh& m, Xoshiro256& rng, int extraGens = 0);

} // namespace chaos
