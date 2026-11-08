#include "shape/parametric.h"
#include <cmath>
#include <numbers>

namespace chaos {

void Mesh::computeVertexNormals() {
    normals.assign(positions.size(), glm::vec3(0));
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const glm::vec3& a = positions[indices[i]];
        const glm::vec3& b = positions[indices[i + 1]];
        const glm::vec3& c = positions[indices[i + 2]];
        glm::vec3 n = glm::cross(c - b, a - b);   // three.js winding: cb x ab
        normals[indices[i]] += n;
        normals[indices[i + 1]] += n;
        normals[indices[i + 2]] += n;
    }
    for (auto& n : normals) {
        float l = glm::length(n);
        n = l > 1e-12f ? n / l : glm::vec3(0, 1, 0);
    }
}

void Mesh::center() {
    if (positions.empty()) return;
    glm::vec3 lo = positions[0], hi = positions[0];
    for (const auto& p : positions) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
    glm::vec3 c = (lo + hi) * 0.5f;
    for (auto& p : positions) p -= c;
}

float Mesh::boundingRadius() const {
    float m = 0;
    for (const auto& p : positions) m = std::max(m, glm::dot(p, p));
    return std::sqrt(m);
}

Mesh parametric(const std::function<glm::vec3(float, float)>& f, int slices, int stacks) {
    Mesh m;
    const int sliceCount = slices + 1;
    m.positions.reserve(size_t(sliceCount) * (stacks + 1));
    for (int i = 0; i <= stacks; i++) {
        float v = float(i) / stacks;
        for (int j = 0; j <= slices; j++) {
            float u = float(j) / slices;
            m.positions.push_back(f(u, v));
        }
    }
    m.indices.reserve(size_t(slices) * stacks * 6);
    for (int i = 0; i < stacks; i++)
        for (int j = 0; j < slices; j++) {
            uint32_t a = i * sliceCount + j;
            uint32_t b = i * sliceCount + j + 1;
            uint32_t c = (i + 1) * sliceCount + j + 1;
            uint32_t d = (i + 1) * sliceCount + j;
            m.indices.insert(m.indices.end(), { a, b, d, b, c, d });
        }
    m.computeVertexNormals();
    return m;
}

Mesh sphere(float radius, int widthSeg, int heightSeg) {
    const float PI = std::numbers::pi_v<float>;
    Mesh m = parametric([&](float u, float v) {
        float theta = v * PI, phi = u * 2 * PI;
        return glm::vec3(-radius * std::cos(phi) * std::sin(theta), radius * std::cos(theta), radius * std::sin(phi) * std::sin(theta));
    }, widthSeg, heightSeg);
    for (size_t i = 0; i < m.positions.size(); i++) m.normals[i] = glm::normalize(m.positions[i]);
    return m;
}

} // namespace chaos
