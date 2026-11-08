#include "shape/primitives.h"
#include <cmath>
#include <algorithm>
#include <numbers>
#include <unordered_map>
#include <array>
#include <glm/gtc/quaternion.hpp>

namespace chaos {

static const float PI = std::numbers::pi_v<float>;

// ---- helpers -----------------------------------------------------------------

static void addQuadGrid(Mesh& m, int cols, int rows, uint32_t base, bool flip = false) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++) {
            uint32_t a = base + i * (cols + 1) + j, b = a + 1, c = a + cols + 1, d = c + 1;
            if (!flip) m.indices.insert(m.indices.end(), { a, c, b, b, c, d });
            else       m.indices.insert(m.indices.end(), { a, b, c, b, d, c });
        }
}

// ---- plane / box ---------------------------------------------------------------

Mesh planeGeo(float w, float h, int ws, int hs) {
    Mesh m;
    for (int iy = 0; iy <= hs; iy++)
        for (int ix = 0; ix <= ws; ix++) {
            m.positions.emplace_back(ix * w / ws - w / 2, h / 2 - iy * h / hs, 0);
            m.normals.emplace_back(0, 0, 1);
        }
    addQuadGrid(m, ws, hs, 0);
    return m;
}

static void boxFace(Mesh& m, int u, int v, int w, float udir, float vdir, float width, float height, float depth, int gx, int gy) {
    uint32_t base = uint32_t(m.positions.size());
    float sw = width / gx, sh = height / gy, hw = width / 2, hh = height / 2, hd = depth / 2;
    for (int iy = 0; iy <= gy; iy++)
        for (int ix = 0; ix <= gx; ix++) {
            glm::vec3 p(0), n(0);
            p[u] = (ix * sw - hw) * udir;
            p[v] = (iy * sh - hh) * vdir;
            p[w] = hd;
            n[w] = depth > 0 ? 1.f : -1.f;
            m.positions.push_back(p);
            m.normals.push_back(n);
        }
    addQuadGrid(m, gx, gy, base);
}

Mesh boxGeo(float w, float h, float d, int ws, int hs, int ds) {
    Mesh m;
    boxFace(m, 2, 1, 0, -1, -1, d, h, w, ds, hs);    // +x
    boxFace(m, 2, 1, 0, 1, -1, d, h, -w, ds, hs);    // -x
    boxFace(m, 0, 2, 1, 1, 1, w, d, h, ws, ds);      // +y
    boxFace(m, 0, 2, 1, 1, -1, w, d, -h, ws, ds);    // -y
    boxFace(m, 0, 1, 2, 1, -1, w, h, d, ws, hs);     // +z
    boxFace(m, 0, 1, 2, -1, -1, w, h, -d, ws, hs);   // -z
    return m;
}

// ---- cylinder / cone -----------------------------------------------------------

Mesh cylinderGeo(float rTop, float rBottom, float height, int radialSeg, int heightSeg, bool open) {
    Mesh m;
    float hh = height / 2, slope = (rBottom - rTop) / height;
    for (int y = 0; y <= heightSeg; y++) {
        float v = float(y) / heightSeg, radius = v * (rBottom - rTop) + rTop;
        for (int x = 0; x <= radialSeg; x++) {
            float u = float(x) / radialSeg, th = u * 2 * PI, s = std::sin(th), c = std::cos(th);
            m.positions.emplace_back(radius * s, -v * height + hh, radius * c);
            m.normals.push_back(glm::normalize(glm::vec3(s, slope, c)));
        }
    }
    addQuadGrid(m, radialSeg, heightSeg, 0);
    if (!open) {
        for (int top = 1; top >= 0; top--) {
            float radius = top ? rTop : rBottom;
            if (radius <= 0) continue;
            float sign = top ? 1.f : -1.f;
            uint32_t center = uint32_t(m.positions.size());
            m.positions.emplace_back(0, hh * sign, 0);
            m.normals.emplace_back(0, sign, 0);
            uint32_t ringStart = uint32_t(m.positions.size());
            for (int x = 0; x <= radialSeg; x++) {
                float th = float(x) / radialSeg * 2 * PI;
                m.positions.emplace_back(radius * std::sin(th), hh * sign, radius * std::cos(th));
                m.normals.emplace_back(0, sign, 0);
            }
            for (int x = 0; x < radialSeg; x++) {
                if (top) m.indices.insert(m.indices.end(), { ringStart + x, ringStart + x + 1, center });
                else     m.indices.insert(m.indices.end(), { ringStart + x + 1, ringStart + x, center });
            }
        }
    }
    return m;
}

// ---- ring / torus / knot ------------------------------------------------------

Mesh ringGeo(float inner, float outer, int thetaSeg, int phiSeg) {
    Mesh m;
    for (int j = 0; j <= phiSeg; j++) {
        float radius = inner + (outer - inner) * j / phiSeg;
        for (int i = 0; i <= thetaSeg; i++) {
            float th = float(i) / thetaSeg * 2 * PI;
            m.positions.emplace_back(radius * std::cos(th), radius * std::sin(th), 0);
            m.normals.emplace_back(0, 0, 1);
        }
    }
    addQuadGrid(m, thetaSeg, phiSeg, 0, true);
    return m;
}

Mesh torusGeo(float r, float tube, int radialSeg, int tubularSeg, float arc) {
    Mesh m;
    for (int j = 0; j <= radialSeg; j++)
        for (int i = 0; i <= tubularSeg; i++) {
            float u = float(i) / tubularSeg * arc, v = float(j) / radialSeg * 2 * PI;
            glm::vec3 p((r + tube * std::cos(v)) * std::cos(u), (r + tube * std::cos(v)) * std::sin(u), tube * std::sin(v));
            glm::vec3 c(r * std::cos(u), r * std::sin(u), 0);
            m.positions.push_back(p);
            m.normals.push_back(glm::normalize(p - c));
        }
    addQuadGrid(m, tubularSeg, radialSeg, 0, true);
    return m;
}

static glm::vec3 knotPoint(float u, int p, int q, float radius) {
    float cu = std::cos(u), su = std::sin(u), qu = float(q) / p * u, cs = std::cos(qu);
    return glm::vec3(radius * (2 + cs) * 0.5f * cu, radius * (2 + cs) * su * 0.5f, radius * std::sin(qu) * 0.5f);
}

Mesh torusKnotGeo(float radius, float tube, int tubularSeg, int radialSeg, int p, int q) {
    Mesh m;
    for (int i = 0; i <= tubularSeg; i++) {
        float u = float(i) / tubularSeg * p * 2 * PI;
        glm::vec3 P1 = knotPoint(u, p, q, radius), P2 = knotPoint(u + 0.01f, p, q, radius);
        glm::vec3 T = P2 - P1, N = P2 + P1, B = glm::normalize(glm::cross(T, N));
        N = glm::normalize(glm::cross(B, T));
        for (int j = 0; j <= radialSeg; j++) {
            float v = float(j) / radialSeg * 2 * PI, cx = -tube * std::cos(v), cy = tube * std::sin(v);
            glm::vec3 pos = P1 + cx * N + cy * B;
            m.positions.push_back(pos);
            m.normals.push_back(glm::normalize(pos - P1));
        }
    }
    addQuadGrid(m, radialSeg, tubularSeg, 0, true);
    return m;
}

// ---- lathe -------------------------------------------------------------------

Mesh latheGeo(const std::vector<glm::vec2>& pts, int segments) {
    Mesh m;
    for (int i = 0; i <= segments; i++) {
        float phi = float(i) / segments * 2 * PI, s = std::sin(phi), c = std::cos(phi);
        for (const auto& pt : pts) {
            m.positions.emplace_back(pt.x * s, pt.y, pt.x * c);
        }
    }
    int n = int(pts.size());
    for (int i = 0; i < segments; i++)
        for (int j = 0; j < n - 1; j++) {
            uint32_t a = i * n + j, b = a + n, c = a + n + 1, d = a + 1;
            m.indices.insert(m.indices.end(), { a, b, d, c, d, b });
        }
    m.computeVertexNormals();
    return m;
}

// ---- curves and tubes ----------------------------------------------------------

std::vector<glm::vec3> catmullRom(const std::vector<glm::vec3>& pts, int n, bool closed) {
    std::vector<glm::vec3> out;
    int L = int(pts.size());
    if (L < 2) return out;
    out.reserve(n + 1);
    int segs = closed ? L : L - 1;
    for (int k = 0; k <= n; k++) {
        float t = float(k) / n * segs;
        int i = std::min(int(std::floor(t)), segs - 1);
        float w = t - i;
        auto at = [&](int idx) {
            if (closed) return pts[((idx % L) + L) % L];
            return pts[std::clamp(idx, 0, L - 1)];
        };
        glm::vec3 p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
        // centripetal parameterisation (three.js default)
        auto dt = [](const glm::vec3& a, const glm::vec3& b) { return std::pow(std::max(glm::dot(b - a, b - a), 1e-6f), 0.25f); };
        float dt0 = dt(p0, p1), dt1 = dt(p1, p2), dt2 = dt(p2, p3);
        glm::vec3 t1 = ((p1 - p0) / dt0 - (p2 - p0) / (dt0 + dt1) + (p2 - p1) / dt1) * dt1;
        glm::vec3 t2 = ((p2 - p1) / dt1 - (p3 - p1) / (dt1 + dt2) + (p3 - p2) / dt2) * dt1;
        float w2 = w * w, w3 = w2 * w;
        glm::vec3 c0 = p1, c1 = t1, c2 = -3.f * p1 + 3.f * p2 - 2.f * t1 - t2, c3 = 2.f * p1 - 2.f * p2 + t1 + t2;
        out.push_back(c0 + c1 * w + c2 * w2 + c3 * w3);
    }
    return out;
}

Mesh tubeGeo(const std::vector<glm::vec3>& path, int tubularSeg, float radius, int radialSeg, bool closed) {
    Mesh m;
    int n = int(path.size());
    if (n < 2) return m;
    // parallel transport frames
    std::vector<glm::vec3> T(n), N(n), B(n);
    for (int i = 0; i < n; i++) {
        glm::vec3 a = path[std::max(i - 1, 0)], b = path[std::min(i + 1, n - 1)];
        T[i] = glm::normalize(b - a);
    }
    glm::vec3 up = std::abs(T[0].y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    N[0] = glm::normalize(glm::cross(T[0], up));
    B[0] = glm::cross(T[0], N[0]);
    for (int i = 1; i < n; i++) {
        glm::vec3 axis = glm::cross(T[i - 1], T[i]);
        float l = glm::length(axis);
        N[i] = N[i - 1];
        if (l > 1e-6f) {
            axis /= l;
            float ang = std::acos(std::clamp(glm::dot(T[i - 1], T[i]), -1.f, 1.f));
            N[i] = glm::angleAxis(ang, axis) * N[i - 1];
        }
        B[i] = glm::cross(T[i], N[i]);
    }
    (void)tubularSeg; // the path is already sampled at tubularSeg + 1 points by the caller
    for (int i = 0; i < n; i++)
        for (int j = 0; j <= radialSeg; j++) {
            float v = float(j) / radialSeg * 2 * PI, s = std::sin(v), c = -std::cos(v);
            glm::vec3 nrm = glm::normalize(c * N[i] + s * B[i]);
            m.positions.push_back(path[i] + radius * nrm);
            m.normals.push_back(nrm);
        }
    addQuadGrid(m, radialSeg, n - 1, 0, true);
    (void)closed;
    return m;
}

// ---- polyhedra -----------------------------------------------------------------

static Mesh polyhedron(const std::vector<glm::vec3>& verts, const std::vector<uint32_t>& idx, float radius, int detail) {
    Mesh m;
    for (size_t f = 0; f + 2 < idx.size(); f += 3) {
        glm::vec3 a = verts[idx[f]], b = verts[idx[f + 1]], c = verts[idx[f + 2]];
        int cols = detail + 1;
        std::vector<std::vector<glm::vec3>> rows(cols + 1);
        for (int i = 0; i <= cols; i++) {
            glm::vec3 aj = glm::mix(a, c, float(i) / cols), bj = glm::mix(b, c, float(i) / cols);
            int r = cols - i;
            for (int j = 0; j <= r; j++) rows[i].push_back(j == 0 && i == cols ? aj : glm::mix(aj, bj, float(j) / r));
        }
        for (int i = 0; i < cols; i++)
            for (int j = 0; j < 2 * (cols - i) - 1; j++) {
                int k = j / 2;
                uint32_t base = uint32_t(m.positions.size());
                glm::vec3 tri[3];
                if (j % 2 == 0) { tri[0] = rows[i][k + 1]; tri[1] = rows[i + 1][k]; tri[2] = rows[i][k]; }
                else { tri[0] = rows[i][k + 1]; tri[1] = rows[i + 1][k + 1]; tri[2] = rows[i + 1][k]; }
                for (auto& p : tri) {
                    glm::vec3 n = glm::normalize(p);
                    m.positions.push_back(n * radius);
                    m.normals.push_back(n);
                }
                m.indices.insert(m.indices.end(), { base, base + 1, base + 2 });
            }
    }
    return weld(m, 1e-4f * std::max(radius, 1.f));
}

Mesh icosahedronGeo(float radius, int detail) {
    const float t = (1 + std::sqrt(5.f)) / 2;
    std::vector<glm::vec3> v = { {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1} };
    std::vector<uint32_t> i = { 0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11, 1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8, 3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9, 4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1 };
    return polyhedron(v, i, radius, detail);
}

Mesh octahedronGeo(float radius, int detail) {
    std::vector<glm::vec3> v = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    std::vector<uint32_t> i = { 0,2,4, 0,4,3, 0,3,5, 0,5,2, 1,2,5, 1,5,3, 1,3,4, 1,4,2 };
    return polyhedron(v, i, radius, detail);
}

Mesh tetrahedronGeo(float radius, int detail) {
    std::vector<glm::vec3> v = { {1,1,1}, {-1,-1,1}, {-1,1,-1}, {1,-1,-1} };
    std::vector<uint32_t> i = { 2,1,0, 0,3,2, 1,3,0, 2,3,1 };
    return polyhedron(v, i, radius, detail);
}

Mesh dodecahedronGeo(float radius, int detail) {
    const float t = (1 + std::sqrt(5.f)) / 2, r = 1 / t;
    std::vector<glm::vec3> v = { {-1,-1,-1}, {-1,-1,1}, {-1,1,-1}, {-1,1,1}, {1,-1,-1}, {1,-1,1}, {1,1,-1}, {1,1,1},
        {0,-r,-t}, {0,-r,t}, {0,r,-t}, {0,r,t}, {-r,-t,0}, {-r,t,0}, {r,-t,0}, {r,t,0}, {-t,0,-r}, {t,0,-r}, {-t,0,r}, {t,0,r} };
    std::vector<uint32_t> i = { 3,11,7, 3,7,15, 3,15,13, 7,19,17, 7,17,6, 7,6,15, 17,4,8, 17,8,10, 17,10,6, 8,0,16, 8,16,2, 8,2,10,
        0,12,1, 0,1,18, 0,18,16, 6,10,2, 6,2,13, 6,13,15, 2,16,18, 2,18,3, 2,3,13, 18,1,9, 18,9,11, 18,11,3,
        4,14,12, 4,12,0, 4,0,8, 11,9,5, 11,5,19, 11,19,7, 19,5,14, 19,14,4, 19,4,17, 1,12,14, 1,14,5, 1,5,9 };
    return polyhedron(v, i, radius, detail);
}

// ---- utils ---------------------------------------------------------------------

Mesh mergeMeshes(const std::vector<Mesh>& parts) {
    Mesh out;
    for (const auto& p : parts) {
        uint32_t base = uint32_t(out.positions.size());
        out.positions.insert(out.positions.end(), p.positions.begin(), p.positions.end());
        out.normals.insert(out.normals.end(), p.normals.begin(), p.normals.end());
        for (uint32_t i : p.indices) out.indices.push_back(base + i);
    }
    return out;
}

void transformMesh(Mesh& m, const glm::mat4& mat) {
    glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(mat)));
    for (auto& p : m.positions) p = glm::vec3(mat * glm::vec4(p, 1));
    for (auto& n : m.normals) n = glm::normalize(nm * n);
}

void translateMesh(Mesh& m, const glm::vec3& t) { for (auto& p : m.positions) p += t; }

Mesh weld(const Mesh& m, float eps) {
    struct Key { int64_t x, y, z; bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; } };
    struct H { size_t operator()(const Key& k) const { return size_t(k.x * 73856093LL ^ k.y * 19349663LL ^ k.z * 83492791LL); } };
    std::unordered_map<Key, uint32_t, H> seen;
    std::vector<uint32_t> remap(m.positions.size());
    Mesh out;
    float inv = 1.f / eps;
    for (size_t i = 0; i < m.positions.size(); i++) {
        const auto& p = m.positions[i];
        Key k{ int64_t(std::llround(p.x * inv)), int64_t(std::llround(p.y * inv)), int64_t(std::llround(p.z * inv)) };
        auto it = seen.find(k);
        if (it == seen.end()) {
            seen.emplace(k, uint32_t(out.positions.size()));
            remap[i] = uint32_t(out.positions.size());
            out.positions.push_back(p);
        } else remap[i] = it->second;
    }
    out.indices.reserve(m.indices.size());
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        uint32_t a = remap[m.indices[i]], b = remap[m.indices[i + 1]], c = remap[m.indices[i + 2]];
        if (a == b || b == c || a == c) continue;    // degenerate after the snap
        out.indices.insert(out.indices.end(), { a, b, c });
    }
    out.computeVertexNormals();
    return out;
}

} // namespace chaos
