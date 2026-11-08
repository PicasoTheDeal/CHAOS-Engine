#include "shape/superellipsoid.h"
#include <cmath>
#include <numbers>

namespace chaos {

SuperellipsoidParams rollSuperellipsoid(Xoshiro256& rng, int slices, int stacks) {
    SuperellipsoidParams p;
    p.e1 = rng.range(0.2f, 2.5f);
    p.e2 = rng.range(0.2f, 2.5f);
    p.ax = rng.range(0.8f, 1.8f);
    p.ay = rng.range(0.8f, 1.8f);
    p.az = rng.range(0.8f, 1.8f);
    p.slices = slices;
    p.stacks = stacks;
    return p;
}

static inline float sp(float w, float e) {
    float s = w > 0 ? 1.f : (w < 0 ? -1.f : 0.f);
    return s * std::pow(std::fabs(w), e);
}

Mesh buildSuperellipsoid(const SuperellipsoidParams& p, float R) {
    const float PI = std::numbers::pi_v<float>;
    return parametric([&](float u, float v) {
        float th = (u - 0.5f) * PI, ph = (v - 0.5f) * PI * 2;
        return glm::vec3(R * p.ax * sp(std::cos(th), p.e1) * sp(std::cos(ph), p.e2),
                         R * p.ay * sp(std::sin(th), p.e1),
                         R * p.az * sp(std::cos(th), p.e1) * sp(std::sin(ph), p.e2));
    }, p.slices, p.stacks);
}

} // namespace chaos
