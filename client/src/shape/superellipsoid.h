#pragma once
#include "shape/parametric.h"
#include "seed/rng.h"

namespace chaos {

// Family 31 in the reference: one family that runs from octahedron through sphere to rounded box to star.
struct SuperellipsoidParams {
    float e1, e2, ax, ay, az;
    int slices, stacks;
};
SuperellipsoidParams rollSuperellipsoid(Xoshiro256& rng, int slices, int stacks);
Mesh buildSuperellipsoid(const SuperellipsoidParams& p, float radius);

} // namespace chaos
