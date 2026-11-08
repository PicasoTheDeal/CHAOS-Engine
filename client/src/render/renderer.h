#pragma once
// uploads a Dimension to the GPU and draws it: scene into the post stack, post stack into the target.
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "scene/dimension.h"
#include "render/shader.h"
#include "render/gl_mesh.h"
#include "render/post.h"

namespace chaos {

// what this install chooses to draw. the dimension is untouched (its value, lineage and seed stay whole);
// a wallpaper that wants only the main shape just does not draw the rest. all on by default.
struct Layers {
    bool satellites = true;     // the orbiting berries / glitch shards
    bool swarm = true;          // rings, debris, fold planes
    bool particles = true;      // the dust
};

Layers layersFromName(const std::string& name);   // "" everything, "nosats" no satellites, "main" the main shape alone

// how much the stack costs: the scene and post run at scale x the window, with this many msaa samples (0 / 2 / 4)
struct Quality {
    float scale = 1.f;
    int samples = 2;
    bool lite = false;      // lite: background noise at 3 octaves instead of 5, bloom chain from quarter res
};

class Renderer {
public:
    Renderer(Dimension& d, int w, int h);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void resize(int w, int h);                         // the window (or target) size. the stack's own size follows quality().scale
    void frame(float t, float dt, unsigned targetFbo);
    void setLayers(const Layers& l) { layers_ = l; }
    const Layers& layers() const { return layers_; }
    void setQuality(const Quality& q);
    const Quality& quality() const { return quality_; }
private:
    void feedBerry(Shader& sh, const Skin& k, const glm::mat4& model);
    void setPhongLights(Shader& sh, const glm::mat4& view);
    Dimension& d_;
    Layers layers_;
    Quality quality_;
    int winW_ = 0, winH_ = 0;
    Post post_;
    Shader berry_, bg_, basic_, glitch_, phong_, own_, points_;
    unsigned ownTex_ = 0;                  // the model's base-colour texture, when it brought one
    GLMesh main_, dome_;
    std::vector<std::unique_ptr<GLMesh>> sats_, planes_, debris_, rings_;
    GLPoints particles_;
    std::vector<glm::vec3> partPos_;
    glm::vec3 camPos_;
    glm::mat4 proj_, view_;
    // per-frame derived colours
    glm::vec3 skinA_, skinB_, trans_, back_, amb_;
    glm::vec3 lightDirM_[3], lightCol_[3];
};

} // namespace chaos
