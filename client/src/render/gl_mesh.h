#pragma once
#include "shape/parametric.h"

namespace chaos {

// VAO + interleaved VBO (pos, normal, and when the mesh has them uv, colour) + EBO.
// attribute 0 = position, 1 = normal, 2 = uv (vec2), 3 = colour (vec4). absent streams are not enabled.
class GLMesh {
public:
    explicit GLMesh(const Mesh& m);
    ~GLMesh();
    GLMesh(const GLMesh&) = delete;
    GLMesh& operator=(const GLMesh&) = delete;
    void draw() const;
private:
    unsigned vao_ = 0, vbo_ = 0, ebo_ = 0;
    int count_ = 0;
};

// dynamic point cloud: position (attr 0) + brightness (attr 1). positions re-uploaded each frame.
class GLPoints {
public:
    GLPoints(const std::vector<glm::vec3>& pos, const std::vector<float>& bright);
    ~GLPoints();
    GLPoints(const GLPoints&) = delete;
    GLPoints& operator=(const GLPoints&) = delete;
    void updatePositions(const std::vector<glm::vec3>& pos);
    void draw() const;
private:
    unsigned vao_ = 0, vboPos_ = 0, vboBright_ = 0;
    int count_ = 0;
};

} // namespace chaos
