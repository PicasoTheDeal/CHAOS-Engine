#include "shape/mutate.h"
#include <cmath>
#include <algorithm>
#include <numbers>
#include <functional>
#include <glm/gtx/rotate_vector.hpp>

namespace chaos {

static const float PI = std::numbers::pi_v<float>;

namespace {

struct Ctx {
    Mesh& m;
    Xoshiro256& r;
    float nSeed;
    float radius() const { return std::max(m.boundingRadius(), 1e-6f); }
    float rr(float a, float b) { return r.range(a, b); }
    glm::vec3 axis() { return glm::normalize(glm::vec3(rr(-1, 1), rr(-1, 1), rr(-1, 1))); }
    float hash3(float x, float y, float z) const { float s = std::sin(x * 127.1f + y * 311.7f + z * 74.7f + nSeed) * 43758.5453f; return s - std::floor(s); }
    float vnoise(float x, float y, float z) const {
        float ix = std::floor(x), iy = std::floor(y), iz = std::floor(z);
        float fx = x - ix, fy = y - iy, fz = z - iz;
        fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
        auto l = [](float p, float q, float t) { return p + (q - p) * t; };
        return l(l(l(hash3(ix, iy, iz), hash3(ix + 1, iy, iz), fx), l(hash3(ix, iy + 1, iz), hash3(ix + 1, iy + 1, iz), fx), fy),
                 l(l(hash3(ix, iy, iz + 1), hash3(ix + 1, iy, iz + 1), fx), l(hash3(ix, iy + 1, iz + 1), hash3(ix + 1, iy + 1, iz + 1), fx), fy), fz);
    }
};

std::string twist(Ctx& c, float Rr, float k) {
    float tK = 2.5f / std::max(1.5f, Rr) * k, tX = c.rr(-tK, tK), tY = c.rr(-tK, tK);
    for (auto& p : c.m.positions) {
        float x = p.x, y = p.y, z = p.z;
        float aY = z * tY * 0.2f, aX = y * tX * 0.2f;
        float nx = x * std::cos(aY) - z * std::sin(aY), nz = x * std::sin(aY) + z * std::cos(aY); x = nx; z = nz;
        float ny = y * std::cos(aX) - z * std::sin(aX); nz = y * std::sin(aX) + z * std::cos(aX); y = ny; z = nz;
        p = { x, y, z };
    }
    return "twist";
}

std::string warp(Ctx& c, float Rr, float k) {
    float amp = c.rr(0.1f, 0.9f) * k * std::max(1.f, Rr * 0.3f), f = c.rr(0.4f, 1.6f) / std::max(1.f, Rr * 0.3f);
    for (auto& p : c.m.positions) {
        glm::vec3 q = p;
        p.x += std::sin(q.y * f + q.z * 0.7f) * amp;
        p.y += std::sin(q.z * f + q.x * 0.7f) * amp;
        p.z += std::sin(q.x * f + q.y * 0.7f) * amp;
    }
    return "warp";
}

std::string ripple(Ctx& c, float Rr, float k) {
    float pX = c.rr(0.5f, 4.5f), pY = c.rr(0.5f, 4.5f), pZ = c.rr(0.5f, 4.5f), fA = c.rr(1, 8), fB = c.rr(1, 8);
    float dAmp = c.rr(0.02f, 0.14f) * Rr * k; int order = c.r.rangeInt(1, 6);
    for (auto& p : c.m.positions) {
        float dist = glm::length(p); if (dist == 0) dist = 1;
        float sh = std::sin(order * std::atan2(p.y, p.x)) * std::cos(order * std::acos(std::clamp(p.z / dist, -1.f, 1.f)));
        float disp = ((std::sin(p.x * pX) * std::cos(p.y * pY) * std::sin(p.z * pZ) + sh * 0.4f) * fA * 0.25f + std::cos(dist * fB) * 0.5f) * dAmp;
        p += p / dist * disp;
    }
    return "ripple";
}

std::string noise(Ctx& c, float Rr, float k) {
    float f = c.rr(0.6f, 2.2f) / std::max(1.f, Rr * 0.5f), amp = c.rr(0.08f, 0.3f) * Rr * k, off = c.rr(0, 50);
    for (auto& p : c.m.positions) {
        float dist = glm::length(p); if (dist == 0) dist = 1;
        float n = (c.vnoise(p.x * f + off, p.y * f, p.z * f) - 0.5f) * 2 * amp;
        p += p / dist * n;
    }
    return "noise";
}

std::string swirl(Ctx& c, float Rr, float k) {
    glm::vec3 ax = c.axis(); float turns = c.rr(-1.2f, 1.2f) * k;
    for (auto& p : c.m.positions) {
        glm::vec3 along = ax * glm::dot(p, ax);
        float radial = glm::length(p - along) / Rr;
        p = glm::rotate(p, radial * turns * PI, ax);
    }
    return "swirl";
}

std::string bulge(Ctx& c, float Rr, float k) {
    glm::vec3 ax = c.axis(); float amt = (c.r.uniform() < 0.5 ? 1 : -1) * c.rr(0.2f, 0.6f) * k, phase = c.rr(0, PI);
    for (auto& p : c.m.positions) {
        float along = glm::dot(p, ax), s = 1 + amt * std::sin(along / Rr * PI + phase);
        glm::vec3 a = ax * along;
        p = (p - a) * s + a;
    }
    return amt > 0 ? "bulge" : "pinch";
}

std::string shear(Ctx& c, float, float k) {
    float sa = c.rr(-0.5f, 0.5f) * k, sb = c.rr(-0.5f, 0.5f) * k;
    for (auto& p : c.m.positions) { p.x += p.y * sa; p.z += p.y * sb; }
    return "shear";
}

std::string fold(Ctx& c, float Rr, float k) {
    glm::vec3 nrm = c.axis(); float d = c.rr(-0.4f, 0.4f) * Rr * k;
    for (auto& p : c.m.positions) {
        float s = glm::dot(p, nrm) + d;
        if (s < 0) p += nrm * (-2 * s);
    }
    return "fold";
}

using Mut = std::string (*)(Ctx&, float, float);
const Mut ALL[] = { twist, warp, ripple, noise, swirl, bulge, shear, fold };

} // namespace

std::vector<std::string> mutateLineage(Mesh& m, Xoshiro256& r, int extraGens) {
    Ctx c{ m, r, r.range(1, 1000) };
    std::vector<std::string> lineage;
    lineage.push_back(twist(c, c.radius(), 1));
    if (r.uniform() < 0.6) lineage.push_back(warp(c, c.radius(), 1));
    lineage.push_back(ripple(c, c.radius(), 1));
    int extra = r.rangeInt(1, 4) + extraGens;
    for (int gen = 0; gen < extra; gen++) lineage.push_back(ALL[r.rangeInt(0, 7)](c, c.radius(), 0.6f + 0.3f * gen));
    m.computeVertexNormals();
    return lineage;
}

} // namespace chaos
