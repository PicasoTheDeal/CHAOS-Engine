#pragma once
#include <vector>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

namespace chaos {

// what a model brought with it (see model/cheng.h): one base colour, how rough / metallic it is, a glow, and at most
// one base-colour texture (rgba8, rows top first, glTF's convention). the engine's own shapes carry none of this.
struct Material {
    glm::vec3 base{ 1.f };
    float roughness = 0.6f, metallic = 0.f;
    glm::vec3 emissive{ 0.f };
    int texW = 0, texH = 0;
    std::vector<uint8_t> rgba;
    bool hasTexture() const { return texW > 0 && texH > 0 && rgba.size() == size_t(texW) * size_t(texH) * 4; }
};

// CPU-side mesh, interleaving left to the GL layer. uvs and colors are optional: empty, or one per vertex.
// a colour's alpha is its texture weight (1 = sample the texture here, 0 = the colour alone): that is how a model
// with several materials but one texture keeps its parts right.
struct Mesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t>  indices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec4> colors;
    Material material;
    bool hasUvs() const { return !uvs.empty() && uvs.size() == positions.size(); }
    bool hasColors() const { return !colors.empty() && colors.size() == positions.size(); }
    bool hasOwnLook() const { return material.hasTexture() || hasColors() || material.base != glm::vec3(1.f) || material.emissive != glm::vec3(0.f); }

    size_t vertexCount() const { return positions.size(); }
    void computeVertexNormals();   // same as three.js BufferGeometry.computeVertexNormals
    void center();                 // translate so the bounding box centre is the origin
    float boundingRadius() const;  // max distance from origin
};

// Port of three.js ParametricGeometry: f(u, v) over [0,1]^2, slices x stacks quads.
Mesh parametric(const std::function<glm::vec3(float, float)>& f, int slices, int stacks);

// Plain UV sphere, used for the backdrop dome.
Mesh sphere(float radius, int widthSeg, int heightSeg);

} // namespace chaos
