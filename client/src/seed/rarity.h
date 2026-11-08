#pragma once
// rarity is rolled, not scored. fixed odds on the "rarity" sub-stream, deterministic from the seed, and the tier
// unlocks visuals. the server recomputes it from the seed alone. ids match enum chaos_tier in shared/chaos_format.h.
#include "seed/rng.h"

namespace chaos {

enum class Tier : int { Common = 0, Uncommon, Rare, Epic, Legendary, Mythic };
constexpr int TIER_COUNT = 6;
const char* tierName(Tier t);
Tier rollTier(Xoshiro256& rng);            // 60 / 25 / 10 / 4 / 0.9 / 0.1 %

// the 41 generators bucketed into 16 families for the .chaos file and the market
constexpr int FAMILY_COUNT = 16;
int familyOf(int shapeType);
const char* familyName(int family);

struct Unlocks {
    int   extraGens;        // added to the mutation lineage
    float satBerryChance;   // 0.55 baseline
    bool  secondBerry;      // a second, smaller berry with its own skin orbiting the main one
    bool  thirdStop;        // duotone grows a third colour between A and B
    bool  forceCompound;    // shape 40
    float afterDampBonus;
    bool  forceBleed;       // native mode becomes bleed
    bool  acidOnly;         // palette family pinned to acid
    int   ringMult;
    bool  gold;             // the seed display turns gold (ui, later)
};
Unlocks unlocksFor(Tier t);

// tier + small deterministic bonuses. only for sorting the market, never for the tier itself.
float rarityScore(Tier t, int lineageLen, int satCount, float mood);

} // namespace chaos
