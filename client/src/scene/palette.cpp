#include "scene/palette.h"
#include <cmath>

namespace chaos {

static float hue2rgb(float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.f / 6) return p + (q - p) * 6 * t;
    if (t < 1.f / 2) return q;
    if (t < 2.f / 3) return p + (q - p) * 6 * (2.f / 3 - t);
    return p;
}

glm::vec3 hslToRgb(float h, float s, float l) {
    h = h - std::floor(h);
    if (s == 0) return glm::vec3(l);
    float q = l <= 0.5f ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    return glm::vec3(hue2rgb(p, q, h + 1.f / 3), hue2rgb(p, q, h), hue2rgb(p, q, h - 1.f / 3));
}

static const float HUE_GAPS[] = { 0.5f, 0.5f, 0.33f, 0.42f, 0.08f, 0.12f, 0.62f, 0.25f };

PalettePair makePair(Xoshiro256& r, int forcedMode) {
    auto u = [&]() { return float(r.uniform()); };
    float h1 = u();
    float gap = HUE_GAPS[int(u() * 8)];
    float h2 = std::fmod(h1 + gap + (u() - 0.5f) * 0.06f + 1.f, 1.f);
    float mode = u();
    if (forcedMode >= 0) mode = 0.125f + 0.2f * forcedMode;   // lands inside the chosen band
    if (forcedMode == 4) mode = 0.9f;
    PalettePair p;
    if (mode < 0.25f) {            // ink + neon
        p.a = hslToRgb(h1, 0.55f + 0.45f * u(), 0.08f + 0.12f * u());
        p.b = hslToRgb(h2, 0.85f + 0.15f * u(), 0.55f + 0.25f * u());
    } else if (mode < 0.5f) {      // dusk pair
        p.a = hslToRgb(h1, 0.6f + 0.3f * u(), 0.18f + 0.17f * u());
        p.b = hslToRgb(h2, 0.5f + 0.4f * u(), 0.62f + 0.25f * u());
    } else if (mode < 0.7f) {      // near-monochrome
        p.a = hslToRgb(h1, 0.5f + 0.5f * u(), 0.12f + 0.15f * u());
        p.b = hslToRgb(std::fmod(h1 + (u() - 0.5f) * 0.05f + 1.f, 1.f), 0.4f + 0.5f * u(), 0.7f + 0.22f * u());
    } else if (mode < 0.85f) {     // bleached / paper
        p.a = hslToRgb(h1, 0.7f + 0.3f * u(), 0.22f + 0.2f * u());
        p.b = hslToRgb(h2, 0.15f + 0.3f * u(), 0.85f + 0.12f * u());
    } else {                       // acid
        p.a = hslToRgb(h1, 0.95f, 0.3f + 0.2f * u());
        p.b = hslToRgb(h2, 0.95f, 0.5f + 0.2f * u());
    }
    return p;
}

glm::vec3 makeThirdStop(Xoshiro256& r, const PalettePair& p) {
    auto u = [&]() { return float(r.uniform()); };
    // between the two in lightness, its own hue so it reads as a third colour
    glm::vec3 mid = (p.a + p.b) * 0.5f;
    float l = 0.3f + 0.4f * (mid.r * 0.2126f + mid.g * 0.7152f + mid.b * 0.0722f);
    return hslToRgb(u(), 0.6f + 0.4f * u(), l);
}

} // namespace chaos
