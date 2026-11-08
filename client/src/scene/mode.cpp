#include "scene/mode.h"
#include <cstring>

namespace chaos {

static const char* NAMES[MODE_COUNT] = { "ZEN", "CALM", "CHAOS", "BLEED" };

const char* modeName(Mode m) { return NAMES[int(m) & 3]; }

bool parseMode(std::string_view s, Mode& out) {
    static const char* lower[MODE_COUNT] = { "zen", "calm", "chaos", "bleed" };
    for (int i = 0; i < MODE_COUNT; i++)
        if (s == lower[i] || (s.size() == 1 && s[0] == '0' + i)) { out = Mode(i); return true; }
    return false;
}

Mode rollMode(Xoshiro256& r) {
    double u = r.uniform();
    if (u < 0.20) return Mode::Zen;
    if (u < 0.55) return Mode::Calm;
    if (u < 0.90) return Mode::Chaos;
    return Mode::Bleed;
}

const ModePreset& modePreset(Mode m) {
    //                               pin   cap   hard  damp  jumps  mut   swarm smear
    static const ModePreset P[] = {
        /* zen   */ { 0.0f, 1.0f, 0.0f, 0.80f, false, false, false, 0.0f },
        /* calm  */ { -1.f, 0.35f, 0.0f, -1.f, true,  true,  true,  0.0f },
        /* chaos */ { -1.f, 1.0f, -1.f, -1.f, true,  true,  true,  0.0f },
        /* bleed */ { 0.95f, 1.0f, 0.6f, 0.96f, true,  true,  true,  1.0f },
    };
    return P[int(m) & 3];
}

} // namespace chaos
