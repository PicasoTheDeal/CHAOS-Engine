#pragma once
// render -> bloom -> duotone -> film -> afterimage, all on fullscreen triangles
#include <glm/glm.hpp>
#include <memory>
#include "render/shader.h"

namespace chaos {

struct PostParams {
    float bloomStrength = 0.2f, bloomRadius = 0.35f, bloomThreshold = 0.65f;
    glm::vec3 colA, colB, colC;
    bool tritone = false;
    float exposure = 0.6f, contrast = 1.2f, split = 0.5f, grainT = 0, grain = 0.006f, kick = 0;
    float filmIntensity = 0.06f;
    float afterDamp = 0.86f;
    float smear = 0;          // > 0 inserts the bleed smear pass after the duotone
};
// the stack, pass by pass: scene (msaa or not) -> resolve -> bright (half or quarter res) -> 5 blurred mips ->
// bloom+duotone in one pass (8 bit from here) -> smear (bleed only) -> afterimage with the film grain folded in ->
// a framebuffer copy to the target. four full-size textures live across frames: the hdr scene, the duotone frame,
// and the afterimage ping-pong. the smear target exists only once bleed has been shown.

class Post {
public:
    Post(int w, int h);
    ~Post();
    void resize(int w, int h);
    void setSamples(int s);                                   // 0, 2 or 4. reallocates when it changes
    int samples() const { return samples_; }
    void setLite(bool lite);                                  // lite: bloom chain starts at quarter res instead of half
    bool lite() const { return lite_; }
    // true colour: the scene target keeps an alpha channel, and pixels a shader wrote with alpha 0 (a model wearing
    // its own textures) skip the duotone ramp and keep their real colour. costs a wider scene target. off by default
    void setTrueColor(bool on);
    bool trueColor() const { return trueColor_; }
    void beginScene();                                        // bind the HDR scene target
    void endScene(unsigned targetFbo, const PostParams& p, int outW = 0, int outH = 0);   // run the stack, final image into targetFbo at outW x outH (0 = internal size)
    int width() const { return w_; }
    int height() const { return h_; }
private:
    void alloc();
    void release();
    void drawFullscreen();
    int w_ = 0, h_ = 0;
    unsigned sceneFbo_ = 0, sceneTex_ = 0;                 // resolved, single sample
    unsigned msFbo_ = 0, msColor_ = 0, msDepth_ = 0;         // multisample render target (samples_ > 0), else msDepth_ backs sceneFbo_
    int samples_ = 2;
    bool lite_ = false, trueColor_ = false;
    unsigned sceneFormat() const;
    unsigned brightFbo_ = 0, brightTex_ = 0;
    static constexpr int MIPS = 5;
    unsigned mipFbo_[MIPS][2] = {}, mipTex_[MIPS][2] = {};
    int mipW_[MIPS] = {}, mipH_[MIPS] = {};
    unsigned duoFbo_ = 0, duoTex_ = 0;
    unsigned smearFbo_ = 0, smearTex_ = 0;
    unsigned afterFbo_[2] = {}, afterTex_[2] = {};
    int afterIdx_ = 0;
    bool afterPrimed_ = false;
    unsigned vao_ = 0;
    std::unique_ptr<Shader> bright_, blur_, grade_, smear_, after_, blit_;
};

} // namespace chaos
