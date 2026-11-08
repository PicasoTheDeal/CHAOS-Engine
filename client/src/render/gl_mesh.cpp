#include "render/gl_mesh.h"
#include <glad/gl.h>

namespace chaos {

GLMesh::GLMesh(const Mesh& m) {
    const bool uv = m.hasUvs(), col = m.hasColors();
    const int stride = 6 + (uv ? 2 : 0) + (col ? 4 : 0);
    std::vector<float> data;
    data.reserve(m.positions.size() * stride);
    for (size_t i = 0; i < m.positions.size(); i++) {
        data.insert(data.end(), { m.positions[i].x, m.positions[i].y, m.positions[i].z, m.normals[i].x, m.normals[i].y, m.normals[i].z });
        if (uv) data.insert(data.end(), { m.uvs[i].x, m.uvs[i].y });
        if (col) data.insert(data.end(), { m.colors[i].r, m.colors[i].g, m.colors[i].b, m.colors[i].a });
    }
    count_ = int(m.indices.size());
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(uint32_t), m.indices.data(), GL_STATIC_DRAW);
    const GLsizei bytes = GLsizei(stride * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, bytes, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, bytes, (void*)(3 * sizeof(float)));
    size_t off = 6;
    if (uv) { glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, bytes, (void*)(off * sizeof(float))); off += 2; }
    if (col) { glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, bytes, (void*)(off * sizeof(float))); }
    glBindVertexArray(0);
}

GLMesh::~GLMesh() {
    glDeleteBuffers(1, &ebo_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void GLMesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

GLPoints::GLPoints(const std::vector<glm::vec3>& pos, const std::vector<float>& bright) {
    count_ = int(pos.size());
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vboPos_);
    glGenBuffers(1, &vboBright_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vboPos_);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(glm::vec3), pos.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vboBright_);
    glBufferData(GL_ARRAY_BUFFER, bright.size() * sizeof(float), bright.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
}

GLPoints::~GLPoints() {
    glDeleteBuffers(1, &vboPos_);
    glDeleteBuffers(1, &vboBright_);
    glDeleteVertexArrays(1, &vao_);
}

void GLPoints::updatePositions(const std::vector<glm::vec3>& pos) {
    glBindBuffer(GL_ARRAY_BUFFER, vboPos_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, pos.size() * sizeof(glm::vec3), pos.data());
}

void GLPoints::draw() const {
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, count_);
    glBindVertexArray(0);
}

} // namespace chaos
