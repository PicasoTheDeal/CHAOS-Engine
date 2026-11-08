#pragma once
// modes are post/mood presets, not different scenes. same seed, same shape, just how much it moves.
// ids match enum chaos_mode in shared/chaos_format.h.
#include "seed/rng.h"
#include <string_view>

namespace chaos {

enum class Mode : int { Zen = 0, Calm = 1, Chaos = 2, Bleed = 3 };
constexpr int MODE_COUNT = 4;

const char* modeName(Mode m);
bool parseMode(std::string_view s, Mode& out);   // "zen" | "calm" | "chaos" | "bleed" | "0".."3"
Mode rollMode(Xoshiro256& rng);                  // native mode, weights 20 / 35 / 35 / 10

struct ModePreset {
    float moodPin;        // < 0: use the seeded mood
    float moodCap;        // upper bound on the seeded mood
    float hardCut;        // < 0: use ml(0, 0.3); otherwise this chance
    float afterDamp;      // < 0: use ml(0.80, 0.94)
    bool  paletteJumps;
    bool  mutation;
    bool  swarm;          // satellites, planes, debris, rings
    float smear;          // bleed only: strength of the chromatic smear pass, 0 = pass skipped
};
const ModePreset& modePreset(Mode m);

} // namespace chaos
