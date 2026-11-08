#include "seed/rarity.h"
#include <cmath>
#include <algorithm>

namespace chaos {

static const char* TIERS[TIER_COUNT] = { "common", "uncommon", "rare", "epic", "legendary", "mythic" };
const char* tierName(Tier t) { return TIERS[std::clamp(int(t), 0, TIER_COUNT - 1)]; }

Tier rollTier(Xoshiro256& r) {
    double u = r.uniform();
    if (u < 0.60) return Tier::Common;
    if (u < 0.85) return Tier::Uncommon;
    if (u < 0.95) return Tier::Rare;
    if (u < 0.99) return Tier::Epic;
    if (u < 0.999) return Tier::Legendary;
    return Tier::Mythic;
}

// generator id -> family
static const int FAMILY_OF[41] = {
    0,  // torus knot        -> knots
    1,  // supershape        -> superformula
    2,  // wobble torus      -> tori
    3,  // icosahedron       -> polyhedra
    3,  // octahedron
    3,  // dodecahedron
    3,  // box
    4,  // sphere            -> primitives
    4,  // cylinder
    4,  // cone
    4,  // ring
    4,  // plane
    5,  // lathe             -> lathes
    6,  // spiral shell      -> shells
    7,  // saddle sheet      -> sheets
    6,  // corkscrew
    2,  // fluted torus
    8,  // ribbed cylinder   -> ribbed
    8,  // rippled sphere
    7,  // sinc ripple
    2,  // torus arc
    9,  // spline tube       -> tubes
    5,  // lathe pod
    0,  // lissajous knot
    1,  // harmonic blob
    10, // klein bottle      -> minimal / non-orientable
    10, // mobius band
    11, // berry cluster     -> clusters
    11, // fractal flake
    7,  // gyroid sheet
    6,  // conch
    1,  // superellipsoid
    0,  // braid
    10, // enneper
    10, // dini
    10, // roman surface
    12, // voxel colony      -> colonies
    0,  // clelia spiral
    12, // urchin
    5,  // rose lathe
    13, // compound
};
static const char* FAMILIES[FAMILY_COUNT] = { "knots", "superformula", "tori", "polyhedra", "primitives", "lathes", "shells", "sheets",
    "ribbed", "tubes", "minimal", "clusters", "colonies", "compound", "mythic (reserved)", "reserved" };

int familyOf(int shapeType) { return FAMILY_OF[((shapeType % 41) + 41) % 41]; }
const char* familyName(int f) { return FAMILIES[std::clamp(f, 0, FAMILY_COUNT - 1)]; }

Unlocks unlocksFor(Tier t) {
    Unlocks u{ 0, 0.55f, false, false, false, 0.f, false, false, 1, false };
    int k = int(t);
    if (k >= 1) { u.extraGens += 1; u.satBerryChance = 0.75f; }
    if (k >= 2) { u.secondBerry = true; u.thirdStop = true; }
    if (k >= 3) { u.forceCompound = true; u.extraGens += 2; u.afterDampBonus = 0.03f; }
    if (k >= 4) { u.forceBleed = true; u.acidOnly = true; u.ringMult = 2; }
    if (k >= 5) { u.gold = true; }   // + the mythic-only family, not written yet
    return u;
}

float rarityScore(Tier t, int lineageLen, int satCount, float mood) {
    float s = float(int(t));
    s += 0.05f * std::clamp(lineageLen - 3, 0, 7);           // up to +0.35
    s += 0.02f * std::clamp(satCount - 2, 0, 12);            // up to +0.24
    s += 0.2f * std::fabs(mood - 0.5f) * 2;                  // extremes, up to +0.2
    return s;
}

} // namespace chaos
