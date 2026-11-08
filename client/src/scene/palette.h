#pragma once
#include <glm/glm.hpp>
#include "seed/rng.h"

namespace chaos {

glm::vec3 hslToRgb(float h, float s, float l);   // three.js Color.setHSL, linear-ish output as the reference used it

// the whole dimension is exactly two colours: A (deep) and B (bright).
struct PalettePair { glm::vec3 a, b; };
PalettePair makePair(Xoshiro256& r, int forcedMode = -1);   // 0 ink+neon, 1 dusk, 2 mono, 3 bleached, 4 acid
glm::vec3 makeThirdStop(Xoshiro256& r, const PalettePair& p);   // rare+: a colour between A and B, its own hue

} // namespace chaos
