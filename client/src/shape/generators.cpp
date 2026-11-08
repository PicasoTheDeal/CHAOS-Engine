#include "shape/generators.h"
#include "shape/primitives.h"
#include "shape/superellipsoid.h"
#include <cmath>
#include <numbers>
#include <set>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace chaos {

static const float PI = std::numbers::pi_v<float>;
static const char* NAMES[SHAPE_COUNT] = { "torus knot", "supershape", "wobble torus", "icosahedron", "octahedron", "dodecahedron", "box", "sphere", "cylinder", "cone",
    "ring", "plane", "lathe", "spiral shell", "saddle sheet", "corkscrew", "fluted torus", "ribbed cylinder", "rippled sphere", "sinc ripple",
    "torus arc", "spline tube", "lathe pod", "lissajous knot", "harmonic blob", "klein bottle", "mobius band", "berry cluster", "fractal flake", "gyroid sheet",
    "conch", "superellipsoid", "braid", "enneper", "dini", "roman surface", "voxel colony", "clelia spiral", "urchin", "rose lathe", "compound" };

const char* shapeName(int t) { return NAMES[((t % SHAPE_COUNT) + SHAPE_COUNT) % SHAPE_COUNT]; }

GeoParams rollGeoParams(Xoshiro256& r) {
    GeoParams g;
    g.type = r.rangeInt(0, SHAPE_COUNT - 1);
    g.radius = r.range(1.4f, 5.2f);
    g.tube = r.range(0.2f, 1.3f);
    g.radialSeg = r.rangeInt(8, 64);
    g.tubularSeg = r.rangeInt(8, 64);
    g.p = r.rangeInt(1, 15);
    g.q = r.rangeInt(1, 15);
    g.width = r.range(2.0f, 6.5f);
    g.height = r.range(2.0f, 6.5f);
    g.depth = r.range(2.0f, 6.5f);
    g.detail = r.rangeInt(1, 8);
    g.subSeg = int(r.range(32, 110));
    g.subSeg2 = int(r.range(16, 56));
    return g;
}

static float superformula(float phi, float a, float b, float m, float n1, float n2, float n3) {
    float t1 = std::pow(std::fabs(std::cos(m * phi / 4) / a), n2);
    float t2 = std::pow(std::fabs(std::sin(m * phi / 4) / b), n3);
    float t3 = t1 + t2;
    return t3 == 0 ? 0 : std::pow(t3, -1 / n1);
}


Mesh buildShape(int type, const GeoParams& gp, Xoshiro256& r, int depth) {
    const float R = gp.radius;
    const int subSeg = gp.subSeg, subSeg2 = gp.subSeg2;
    auto rr = [&](float a, float b) { return r.range(a, b); };
    auto ri = [&](int a, int b) { return r.rangeInt(a, b); };

    switch (type) {
    case 0: return torusKnotGeo(R, gp.tube, subSeg * 2, subSeg2, int(rr(1, 8)), int(rr(2, 11)));
    case 1: {   // supershape
        float sa = rr(0.5f, 1.5f), sb = rr(0.5f, 1.5f), sm = rr(2, 12), sn1 = rr(0.3f, 2), sn2 = rr(0.3f, 2), sn3 = rr(0.3f, 2);
        float ta = rr(0.5f, 1.5f), tb = rr(0.5f, 1.5f), tm = rr(1, 9), tn1 = rr(0.3f, 2), tn2 = rr(0.3f, 2), tn3 = rr(0.3f, 2);
        return parametric([&](float u, float v) {
            float th = u * 2 * PI, ph = (v - 0.5f) * PI;
            float r1 = superformula(th, sa, sb, sm, sn1, sn2, sn3), r2 = superformula(ph, ta, tb, tm, tn1, tn2, tn3);
            return glm::vec3(R * r1 * std::cos(th) * r2 * std::cos(ph), R * r1 * std::sin(th) * r2 * std::cos(ph), R * r2 * std::sin(ph));
        }, subSeg, subSeg2);
    }
    case 2: {   // wobble torus
        float n = rr(1, 5), k = rr(0.1f, 0.6f);
        return parametric([&](float u, float v) {
            float u1 = u * 2 * PI, v1 = v * 2 * PI;
            return glm::vec3(R * std::cos(u1) * (1 + std::cos(v1) / n), R * std::sin(u1) * (1 + std::cos(v1) / n), R * std::sin(v1) / n + std::sin(u1 * 3) * k);
        }, subSeg, subSeg2);
    }
    case 3: return icosahedronGeo(R, int(rr(1, 5)));
    case 4: return octahedronGeo(R, int(rr(1, 5)));
    case 5: return dodecahedronGeo(R, int(rr(1, 4)));
    case 6: return boxGeo(gp.width, gp.height, gp.depth, subSeg2, subSeg2, subSeg2);
    case 7: return sphere(R, subSeg, subSeg2);
    case 8: return cylinderGeo(R * rr(0.1f, 1.5f), R * rr(0.1f, 1.5f), gp.height, subSeg, subSeg2);
    case 9: return coneGeo(R, gp.height, subSeg, subSeg2);
    case 10: return ringGeo(R * rr(0.05f, 0.5f), R * rr(1, 2), subSeg, 6);
    case 11: return planeGeo(gp.width, gp.height, subSeg, subSeg);
    case 12: {  // lathe
        std::vector<glm::vec2> pts;
        for (int i = 0; i < 24; i++) {
            float t = i / 23.f;
            pts.emplace_back(std::sin(t * PI * rr(1, 6)) * rr(0.2f, 1.2f) + rr(0.1f, 0.5f), (t - 0.5f) * gp.height * rr(0.8f, 1.5f));
        }
        return latheGeo(pts, subSeg);
    }
    case 13: {  // spiral shell
        float turns = rr(1, 4);
        return parametric([&](float u, float v) {
            float u_ = u * PI * turns, v_ = (v - 0.5f) * PI * 2;
            float rad = R * (1 + 0.3f * std::sin(u_ * 3 + v_ * 2));
            return glm::vec3(rad * std::cos(u_) * std::cos(v_), rad * std::sin(u_) * std::cos(v_), rad * std::sin(v_) + std::cos(u_ * v_) * 0.5f);
        }, subSeg, subSeg2);
    }
    case 14: {  // saddle sheet
        float amp = 0.8f * rr(0.5f, 2), S = R * 0.9f;
        return parametric([&](float u, float v) {
            float u_ = (u * 4 - 2) * S * 0.5f, v_ = (v * 4 - 2) * S * 0.5f;
            return glm::vec3(u_, v_, std::sin(u_ * u_ - v_ * v_) * amp);
        }, subSeg, subSeg);
    }
    case 15: {  // corkscrew
        float c = rr(0.6f, 2.4f);
        return parametric([&](float u, float v) {
            float phi = u * 2 * PI, th = v * PI, k = 1 - phi / (2 * PI);
            return glm::vec3(c * k * std::cos(2 * phi) * (1 + std::cos(th)) + c * std::cos(phi), c * k * std::sin(2 * phi) * (1 + std::cos(th)) + c * std::sin(phi), c * k * std::sin(th));
        }, subSeg, subSeg2);
    }
    case 16: return parametric([&](float u, float v) {   // fluted torus
            float u_ = u * 2 * PI, v_ = v * PI, rad = R * 0.5f;
            return glm::vec3(rad * (2 + std::cos(v_)) * std::cos(u_), rad * (2 + std::cos(v_)) * std::sin(u_), rad * (std::sin(v_) + std::cos(v_ * 4)));
        }, subSeg, subSeg2);
    case 17: return parametric([&](float u, float v) {   // ribbed cylinder
            float u_ = u * 2 * PI, v_ = v * 2 - 1, s = std::sqrt(std::max(0.f, 1 - v_ * v_));
            return glm::vec3(R * s * std::cos(u_), R * s * std::sin(u_), R * v_ + std::sin(u_ * 6) * 0.2f);
        }, subSeg, subSeg2);
    case 18: return parametric([&](float u, float v) {   // rippled sphere
            float u_ = u * 2 * PI, v_ = v * 2 * PI;
            return glm::vec3(std::sin(u_) * std::cos(v_) * R, std::sin(u_) * std::sin(v_) * R, std::cos(u_) * R + std::sin(u_ * 3 + v_ * 3) * 0.4f);
        }, subSeg, subSeg2);
    case 19: {  // sinc ripple
        float f = rr(2, 8), S = R * 1.3f;
        return parametric([&](float u, float v) {
            float u_ = (u * 2 - 1) * S, v_ = (v * 2 - 1) * S, rad = std::sqrt(u_ * u_ + v_ * v_) + 0.001f;
            return glm::vec3(u_, std::sin(rad * f) / rad * 0.8f * S * 0.5f, v_);
        }, subSeg, subSeg);
    }
    case 20: return torusGeo(R * rr(0.5f, 1.5f), gp.tube * rr(0.2f, 2), subSeg2, subSeg, PI * rr(0.5f, 2));
    case 21: {  // spline tube
        std::vector<glm::vec3> pts = {
            { -rr(1, 4), rr(-1, 1), -rr(1, 4) }, { rr(-1, 1), rr(1, 4), rr(-1, 1) }, { rr(1, 4), -rr(-1, 1), rr(1, 4) }, { -rr(1, 4), -rr(1, 4), rr(-1, 1) } };
        return tubeGeo(catmullRom(pts, subSeg, true), subSeg, gp.tube * rr(0.5f, 2), subSeg2, true);
    }
    case 22: {  // lathe pod
        std::vector<glm::vec2> pts;
        for (int i = 0; i < 30; i++) {
            float phi = i / 29.f * PI;
            pts.emplace_back(std::sin(phi) * R + std::cos(phi * 4) * 0.15f + 0.01f, std::cos(phi) * gp.height * 0.5f);
        }
        return latheGeo(pts, subSeg);
    }
    case 23: {  // lissajous knot
        int a = ri(1, 5), b = ri(2, 7), c = ri(1, 6);
        float ph = rr(0, PI), ph2 = rr(0, PI);
        std::vector<glm::vec3> pts;
        for (int i = 0; i < 240; i++) {
            float t = i / 240.f * 2 * PI;
            pts.emplace_back(R * std::sin(a * t + ph), R * std::sin(b * t), R * std::sin(c * t + ph2));
        }
        int segs = std::max(120, subSeg * 2);
        return tubeGeo(catmullRom(pts, segs, true), segs, gp.tube * rr(0.25f, 0.8f), subSeg2, true);
    }
    case 24: {  // harmonic blob
        int m[8]; for (int& x : m) x = ri(0, 7);
        return parametric([&](float u, float v) {
            float th = u * 2 * PI, ph = v * PI;
            float rad = std::pow(std::fabs(std::sin(m[0] * ph)), float(m[1])) + std::pow(std::fabs(std::cos(m[2] * ph)), float(m[3]))
                      + std::pow(std::fabs(std::sin(m[4] * th)), float(m[5])) + std::pow(std::fabs(std::cos(m[6] * th)), float(m[7]));
            rad = R * (0.35f + 0.65f * rad / 4);
            return glm::vec3(rad * std::sin(ph) * std::cos(th), rad * std::cos(ph), rad * std::sin(ph) * std::sin(th));
        }, subSeg, subSeg2);
    }
    case 25: return parametric([&](float u, float v) {   // klein bottle
            u *= PI * 2; v *= 2 * PI;
            float x, y, z;
            if (u < PI) {
                x = 3 * std::cos(u) * (1 + std::sin(u)) + (2 * (1 - std::cos(u) / 2)) * std::cos(u) * std::cos(v);
                z = -8 * std::sin(u) - 2 * (1 - std::cos(u) / 2) * std::sin(u) * std::cos(v);
            } else {
                x = 3 * std::cos(u) * (1 + std::sin(u)) + (2 * (1 - std::cos(u) / 2)) * std::cos(v + PI);
                z = -8 * std::sin(u);
            }
            y = -2 * (1 - std::cos(u) / 2) * std::sin(v);
            return glm::vec3(x, y, z) * (R * 0.16f);
        }, subSeg, subSeg2);
    case 26: {  // mobius band, two offset sheets
        float w = R * rr(0.25f, 0.7f); int twists = ri(1, 5);
        auto sheet = [&](float off) {
            return parametric([&](float u, float v) {
                float t = u * 2 * PI, s = (v - 0.5f) * 2 * w, a = twists * t / 2, rad = R + (s + off) * std::cos(a);
                return glm::vec3(rad * std::cos(t), (s + off) * std::sin(a), rad * std::sin(t));
            }, subSeg * 2, subSeg2);
        };
        return mergeMeshes({ sheet(0), sheet(0.08f * R) });
    }
    case 27: {  // berry cluster
        int n = ri(14, 60); float turns = rr(1.5f, 5), rise = R * rr(0.8f, 2.2f);
        std::vector<Mesh> parts;
        for (int i = 0; i < n; i++) {
            float t = float(i) / n, ang = t * 2 * PI * turns + rr(-0.3f, 0.3f);
            float rad = R * (0.25f + 0.75f * std::sin(t * PI)) * rr(0.7f, 1.1f), br = R * rr(0.16f, 0.42f);
            Mesh s = sphere(br, 22, 16);
            translateMesh(s, { std::cos(ang) * rad, (t - 0.5f) * rise + rr(-0.2f, 0.2f) * R, std::sin(ang) * rad });
            parts.push_back(std::move(s));
        }
        return mergeMeshes(parts);
    }
    case 28: {  // fractal flake
        int baseKind = ri(0, 3), smoothLevel = ri(1, 3);
        auto base = [&]() { switch (baseKind) { case 0: return octahedronGeo(1, 0); case 1: return icosahedronGeo(1, 0); case 2: return tetrahedronGeo(1, 0); default: return dodecahedronGeo(1, 0); } };
        std::vector<Mesh> parts;
        std::function<void(glm::vec3, float, int)> grow = [&](glm::vec3 center, float size, int gen) {
            int k = ri(0, 2);
            Mesh p = k == 0 ? icosahedronGeo(size, smoothLevel) : k == 1 ? octahedronGeo(size, smoothLevel) : sphere(size, 18, 12);
            translateMesh(p, center);
            parts.push_back(std::move(p));
            if (gen == 0 || parts.size() > 400) return;
            Mesh skel = base();
            for (const auto& v : skel.positions) {
                if (r.uniform() < 0.25) continue;
                grow(center + v * (size * 1.35f), size * rr(0.38f, 0.55f), gen - 1);
            }
        };
        grow(glm::vec3(0), R * 0.7f, 2);
        return mergeMeshes(parts);
    }
    case 29: {  // gyroid sheet
        float f = rr(1.2f, 3), S = R * 1.2f;
        return parametric([&](float u, float v) {
            float x = (u * 2 - 1) * S, z = (v * 2 - 1) * S;
            float y = (std::sin(f * x) * std::cos(f * z) + std::sin(f * z) * std::cos(f * x * 0.5f)) * S * 0.35f;
            return glm::vec3(x, y, z + std::sin(f * y) * 0.3f);
        }, subSeg, subSeg);
    }
    case 30: {  // conch
        float turns = rr(2.5f, 6), growth = rr(0.12f, 0.3f), flare = rr(0.5f, 1.4f), pitch = rr(0.3f, 1.2f);
        return parametric([&](float u, float v) {
            float t = u * 2 * PI * turns, s = v * 2 * PI, e = std::exp(growth * t / (2 * PI));
            float rad = R * 0.24f * e * flare, cx = R * 0.36f * e * std::cos(t), cz = R * 0.36f * e * std::sin(t), cy = -R * 0.24f * e * pitch;
            return glm::vec3(cx + rad * std::cos(s) * std::cos(t), cy + rad * std::sin(s), cz + rad * std::cos(s) * std::sin(t));
        }, subSeg * 2, subSeg2);
    }
    case 31: return buildSuperellipsoid(rollSuperellipsoid(r, subSeg, subSeg2), R);
    case 32: {  // braid
        int strands = ri(2, 5); float twist = rr(2, 7), tubeR = gp.tube * rr(0.25f, 0.6f), sep = R * rr(0.15f, 0.35f);
        std::vector<Mesh> parts;
        for (int k = 0; k < strands; k++) {
            std::vector<glm::vec3> pts;
            for (int i = 0; i < 200; i++) {
                float t = i / 200.f * 2 * PI, a = t * twist + k * 2 * PI / strands, rad = R + sep * std::cos(a);
                pts.emplace_back(rad * std::cos(t), sep * std::sin(a), rad * std::sin(t));
            }
            parts.push_back(tubeGeo(catmullRom(pts, 200, true), 200, tubeR, std::max(6, subSeg2 / 2), true));
        }
        return mergeMeshes(parts);
    }
    case 33: {  // enneper
        int k = ri(2, 5); float S = R * 0.55f, rMax = k >= 4 ? 1.15f : 1.5f;
        return parametric([&](float u, float v) {
            float rad = u * rMax, ph = v * 2 * PI, m = float(2 * k - 1);
            float x = rad * std::cos(ph) - std::pow(rad, m) * std::cos(m * ph) / m;
            float y = rad * std::sin(ph) + std::pow(rad, m) * std::sin(m * ph) / m;
            float z = 2 * std::pow(rad, float(k)) * std::cos(k * ph) / k;
            return glm::vec3(x * S, z * S, y * S);
        }, subSeg2, subSeg);
    }
    case 34: {  // dini
        float a = R * 0.5f, b = rr(0.08f, 0.25f), turns = rr(2, 5);
        return parametric([&](float u, float v) {
            float uu = u * 2 * PI * turns, vv = 0.08f + v * (PI - 0.16f) * 0.6f;
            return glm::vec3(a * std::cos(uu) * std::sin(vv), a * (std::cos(vv) + std::log(std::tan(vv / 2))) + b * uu * a * 0.5f, a * std::sin(uu) * std::sin(vv));
        }, subSeg * 2, subSeg2);
    }
    case 35: {  // roman surface
        float S = R * 1.8f, lx = rr(0.7f, 1.4f), ly = rr(0.7f, 1.4f), lz = rr(0.7f, 1.4f);
        return parametric([&](float u, float v) {
            float th = u * 2 * PI, ph = (v - 0.5f) * PI, a = std::cos(th) * std::cos(ph), b = std::sin(th) * std::cos(ph), c = std::sin(ph);
            return glm::vec3(S * b * c * lx, S * a * c * ly, S * a * b * lz);
        }, subSeg, subSeg2);
    }
    case 36: {  // voxel colony
        int N = ri(4, 8); float cell = (R * 1.6f) / N, thresh = rr(0.35f, 0.6f);
        std::vector<uint8_t> live(N * N * N), next(N * N * N);
        for (auto& c : live) c = r.uniform() < thresh ? 1 : 0;
        auto at = [&](int x, int y, int z) { return (x < 0 || y < 0 || z < 0 || x >= N || y >= N || z >= N) ? 0 : live[x + N * (y + N * z)]; };
        for (int z = 0; z < N; z++) for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            int nb = at(x-1,y,z) + at(x+1,y,z) + at(x,y-1,z) + at(x,y+1,z) + at(x,y,z-1) + at(x,y,z+1);
            next[x + N * (y + N * z)] = at(x, y, z) ? (nb >= 1) : (nb >= 3);
        }
        std::vector<Mesh> parts;
        for (int z = 0; z < N && parts.size() < 320; z++) for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            if (!next[x + N * (y + N * z)]) continue;
            float s = cell * rr(0.75f, 1);
            Mesh box = boxGeo(s, s, s, 2, 2, 2);
            translateMesh(box, { (x - (N - 1) / 2.f) * cell, (y - (N - 1) / 2.f) * cell, (z - (N - 1) / 2.f) * cell });
            parts.push_back(std::move(box));
        }
        if (parts.empty()) parts.push_back(boxGeo(R, R, R, 4, 4, 4));
        return mergeMeshes(parts);
    }
    case 37: {  // clelia spiral
        float c = rr(2, 9), tubeR = gp.tube * rr(0.2f, 0.5f);
        std::vector<glm::vec3> pts;
        for (int i = 0; i <= 400; i++) {
            float th = i / 400.f * PI;
            pts.emplace_back(R * std::sin(th) * std::cos(c * th), R * std::cos(th), R * std::sin(th) * std::sin(c * th));
        }
        return tubeGeo(catmullRom(pts, 400, false), 400, tubeR, std::max(6, subSeg2 / 2), false);
    }
    case 38: {  // urchin
        int spikes = ri(30, 140); float len = R * rr(0.6f, 1.6f), base = R * rr(0.04f, 0.12f), core = R * rr(0.35f, 0.7f);
        std::vector<Mesh> parts = { sphere(core, 32, 24) };
        float ga = PI * (3 - std::sqrt(5.f));
        for (int i = 0; i < spikes; i++) {
            float y = 1 - (float(i) / (spikes - 1)) * 2, rad = std::sqrt(std::max(0.f, 1 - y * y)), th = ga * i;
            glm::vec3 dir(std::cos(th) * rad, y, std::sin(th) * rad);
            float h = len * rr(0.5f, 1.3f);
            Mesh cone = coneGeo(base * rr(0.6f, 1.4f), h, 10, 1);
            translateMesh(cone, { 0, h * 0.5f + core * 0.8f, 0 });
            glm::quat q = glm::rotation(glm::vec3(0, 1, 0), dir);
            transformMesh(cone, glm::mat4_cast(q));
            parts.push_back(std::move(cone));
        }
        return mergeMeshes(parts);
    }
    case 39: {  // rose lathe
        int k = ri(2, 7); float ruffle = rr(0, 0.25f), H = gp.height;
        std::vector<glm::vec2> pts;
        for (int i = 0; i < 48; i++) {
            float t = i / 47.f, th = t * PI;
            float rad = std::fabs(std::cos(k * th)) * R * (0.6f + 0.4f * std::sin(t * PI)) + 0.05f * R + std::sin(t * PI * 9) * ruffle * R;
            pts.emplace_back(rad, (t - 0.5f) * H);
        }
        return latheGeo(pts, subSeg);
    }
    case 40:
    default: {  // compound
        if (depth > 0) return torusKnotGeo(R * 0.6f, gp.tube * 0.6f, 96, 20, ri(1, 5), ri(2, 6));
        int n = ri(2, 4);
        std::vector<Mesh> parts;
        for (int i = 0; i < n; i++) {
            int t = ri(0, SHAPE_COUNT - 2);
            if (t == 27 || t == 28 || t == 36 || t == 38) t = ri(0, 26);
            Mesh sub = buildShape(t, gp, r, depth + 1);
            glm::vec3 pos = glm::vec3(rr(-1, 1), rr(-1, 1), rr(-1, 1)) * (R * 0.35f);
            glm::quat q = glm::quat(glm::vec3(rr(0, 6.28f), rr(0, 6.28f), rr(0, 6.28f)));
            float s = rr(0.45f, 0.95f);
            glm::mat4 m = glm::translate(glm::mat4(1), pos) * glm::mat4_cast(q) * glm::scale(glm::mat4(1), glm::vec3(s));
            transformMesh(sub, m);
            parts.push_back(std::move(sub));
        }
        return mergeMeshes(parts);
    }
    }
}

} // namespace chaos
