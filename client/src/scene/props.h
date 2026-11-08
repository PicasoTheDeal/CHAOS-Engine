#pragma once
// What the registry needs from a seed without building a single triangle: tier, family, native mode.
// rollDimension uses exactly this, so the two can never disagree.
#include "seed/seed.h"
#include "seed/rarity.h"
#include "scene/mode.h"
#include "shape/generators.h"

namespace chaos {

struct SeedProps {
    Tier tier;
    Unlocks unlocks;
    Mode nativeMode;
    GeoParams geo;        // the shape stream's roll, before the shape itself is built
    int shapeType;        // after the unlocks (epic+ forces compound)
    int family;
    uint32_t mutationSeed;
};

SeedProps deriveProps(const Seed32& seed, Xoshiro256* shapeRngOut = nullptr);   // hands back the shape stream for the builder

} // namespace chaos
