#include "render/shader.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <unistd.h>

namespace chaos {

std::string Shader::shaderDir() {
    static std::string dir = [] {
        char buf[4096];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            auto exeDir = std::filesystem::path(buf).parent_path();
            for (auto p : { exeDir / "shaders", exeDir / ".." / "share" / "chaos" / "shaders" })
                if (std::filesystem::is_directory(p)) return std::filesystem::weakly_canonical(p).string();
        }
#ifdef CHAOS_DATA_DIR
        if (std::filesystem::is_directory(std::filesystem::path(CHAOS_DATA_DIR) / "shaders")) return (std::filesystem::path(CHAOS_DATA_DIR) / "shaders").string();
#endif
#ifdef CHAOS_SHADER_DIR
        return std::string(CHAOS_SHADER_DIR);
#else
        return std::string("shaders");
#endif
    }();
    return dir;
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read shader " + path);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// "#include <file>" lines are replaced with that file's contents, one level deep. Used for the berry common block.
static std::string preprocess(const std::string& src) {
    std::stringstream in(src), out;
    std::string line;
    while (std::getline(in, line)) {
        auto p = line.find("#include <");
        if (p != std::string::npos) {
            auto e = line.find('>', p);
            std::string name = line.substr(p + 10, e - p - 10);
            out << readFile(Shader::shaderDir() + "/" + name) << "\n";
        } else out << line << "\n";
    }
    return out.str();
}

static GLuint compile(GLenum type, const std::string& src, const std::string& name) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]; glGetShaderInfoLog(s, sizeof log, nullptr, log);
        throw std::runtime_error(name + ": " + log);
    }
    return s;
}

Shader::Shader(const std::string& vertFile, const std::string& fragFile) {
    std::string dir = shaderDir() + "/";
    GLuint vs = compile(GL_VERTEX_SHADER, preprocess(readFile(dir + vertFile)), vertFile);
    GLuint fs = compile(GL_FRAGMENT_SHADER, preprocess(readFile(dir + fragFile)), fragFile);
    prog_ = glCreateProgram();
    glAttachShader(prog_, vs);
    glAttachShader(prog_, fs);
    glLinkProgram(prog_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok; glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096]; glGetProgramInfoLog(prog_, sizeof log, nullptr, log);
        throw std::runtime_error("link " + vertFile + "+" + fragFile + ": " + log);
    }
}

Shader::~Shader() { if (prog_) glDeleteProgram(prog_); }
void Shader::use() const { glUseProgram(prog_); }

int Shader::loc(const char* name) {
    auto it = locs_.find(name);
    if (it != locs_.end()) return it->second;
    int l = glGetUniformLocation(prog_, name);
    locs_.emplace(name, l);
    return l;
}

void Shader::set(const char* n, float v) { glUniform1f(loc(n), v); }
void Shader::set(const char* n, int v) { glUniform1i(loc(n), v); }
void Shader::set(const char* n, const glm::vec2& v) { glUniform2fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::vec3& v) { glUniform3fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::mat4& m) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::set(const char* n, const glm::mat3& m) { glUniformMatrix3fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::setArray(const char* n, const glm::vec3* v, int count) { glUniform3fv(loc(n), count, glm::value_ptr(v[0])); }

} // namespace chaos
