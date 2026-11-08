#pragma once
#include "shape/parametric.h"
#include "seed/rng.h"
#include <string>

namespace chaos {

constexpr int SHAPE_COUNT = 41;   // 0..39 generators, 40 = compound of the others
const char* shapeName(int type);

// getGeoParams() in the reference: rolled once per dimension before the shape is built
struct GeoParams {
    int type;
    float radius, tube;
    int radialSeg, tubularSeg, p, q;
    float width, height, depth;
    int detail;
    int subSeg, subSeg2;
};
GeoParams rollGeoParams(Xoshiro256& rng);

// builds one shape family. `depth` > 0 means we are inside a compound (keeps the vertex budget sane).
Mesh buildShape(int type, const GeoParams& gp, Xoshiro256& rng, int depth = 0);

} // namespace chaos
