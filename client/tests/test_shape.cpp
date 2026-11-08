#include <catch2/catch_test_macros.hpp>
#include "shape/superellipsoid.h"
using namespace chaos;

TEST_CASE("superellipsoid with e=1 is an ellipsoid with unit normals") {
    SuperellipsoidParams p{1, 1, 1, 1, 1, 48, 24};
    Mesh m = buildSuperellipsoid(p, 2.0f);
    REQUIRE(m.vertexCount() == 49u * 25u);
    REQUIRE(m.indices.size() == 48u * 24u * 6u);
    REQUIRE(m.boundingRadius() > 1.99f);
    REQUIRE(m.boundingRadius() < 2.01f);
    for (const auto& n : m.normals) REQUIRE(std::abs(glm::length(n) - 1.f) < 1e-4f);
    // interior vertex: normal points the same way as the position on a sphere
    size_t mid = 12 * 49 + 20;
    REQUIRE(glm::dot(glm::normalize(m.positions[mid]), m.normals[mid]) > 0.95f);
}
