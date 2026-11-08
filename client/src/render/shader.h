#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace chaos {

// one GLSL program loaded from the shaders/ directory. uniform locations are cached by name.
class Shader {
public:
    Shader(const std::string& vertFile, const std::string& fragFile);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void set(const char* name, float v);
    void set(const char* name, int v);
    void set(const char* name, const glm::vec2& v);
    void set(const char* name, const glm::vec3& v);
    void set(const char* name, const glm::mat4& m);
    void set(const char* name, const glm::mat3& m);
    void setArray(const char* name, const glm::vec3* v, int count);

    static std::string shaderDir();   // resolved once: exe-relative shaders/ or the compile-time path

private:
    int loc(const char* name);
    unsigned prog_ = 0;
    std::unordered_map<std::string, int> locs_;
};

} // namespace chaos
