#include "render/post.h"
#include <glad/gl.h>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace chaos {

// gpu time per pass, for the benchmark: CHAOS_PROFILE=1. one query per pass per frame, read back a frame later.
namespace {
struct Prof {
    bool on = std::getenv("CHAOS_PROFILE") != nullptr;
    struct Q { const char* name; unsigned q[2] = { 0, 0 }; double acc = 0; };
    std::vector<Q> qs; int frame = 0; int cur = -1;
    int slot() const { return frame & 1; }
    void begin(const char* name) {
        if (!on) return;
        end();
        int i = -1; for (size_t k = 0; k < qs.size(); k++) if (qs[k].name == name) i = int(k);
        if (i < 0) { qs.push_back({ name }); i = int(qs.size()) - 1; glGenQueries(2, qs[i].q); }
        glBeginQuery(GL_TIME_ELAPSED, qs[i].q[slot()]); cur = i;
    }
    void end() { if (on && cur >= 0) { glEndQuery(GL_TIME_ELAPSED); cur = -1; } }
    void frameDone() {
        if (!on) return;
        end(); frame++;
        if (frame < 2) return;
        for (auto& q : qs) { GLuint64 ns = 0; glGetQueryObjectui64v(q.q[slot()], GL_QUERY_RESULT, &ns); q.acc += ns / 1e6; }
        if (frame % 60 == 0) {
            double total = 0; std::string line = "gpu ms/frame:";
            for (auto& q : qs) { char b[64]; std::snprintf(b, sizeof b, "  %s %.2f", q.name, q.acc / 58.0); line += b; total += q.acc / 58.0; q.acc = 0; }
            char b[32]; std::snprintf(b, sizeof b, "  = %.2f", total); line += b;
            std::fprintf(stderr, "%s\n", line.c_str());
        }
    }
};
Prof& prof() { static Prof p; return p; }
}

// the hdr format: three small floats in 32 bits. half the memory and bandwidth of rgba16f, and nothing in the stack
// ever reads alpha from an hdr target (the bloom threshold and the tone map use rgb only)
static constexpr GLenum HDR = GL_R11F_G11F_B10F;
static unsigned makeTex(int w, int h, GLenum internal) {
    unsigned t; glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    if (internal == HDR) glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, nullptr);
    else glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static unsigned makeFbo(unsigned colorTex, unsigned depthRb = 0) {
    unsigned f; glGenFramebuffers(1, &f);
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
    if (depthRb) glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("post fbo incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return f;
}

Post::Post(int w, int h) {
    bright_ = std::make_unique<Shader>("fs.vert", "bright.frag");
    blur_ = std::make_unique<Shader>("fs.vert", "blur.frag");
    grade_ = std::make_unique<Shader>("fs.vert", "bloom_duotone.frag");
    smear_ = std::make_unique<Shader>("fs.vert", "smear.frag");
    after_ = std::make_unique<Shader>("fs.vert", "afterimage.frag");
    blit_ = std::make_unique<Shader>("fs.vert", "blit.frag");
    glGenVertexArrays(1, &vao_);
    resize(w, h);
}

Post::~Post() { release(); glDeleteVertexArrays(1, &vao_); }

void Post::resize(int w, int h) {
    if (w == w_ && h == h_) return;
    release();
    w_ = w; h_ = h;
    alloc();
}

void Post::setLite(bool lite) { if (lite == lite_) return; lite_ = lite; if (sceneFbo_) { release(); alloc(); } }
void Post::setTrueColor(bool on) { if (on == trueColor_) return; trueColor_ = on; if (sceneFbo_) { release(); alloc(); } }
unsigned Post::sceneFormat() const { return trueColor_ ? GL_RGBA16F : HDR; }

void Post::setSamples(int s) {
    s = s >= 4 ? 4 : s >= 2 ? 2 : 0;
    if (s == samples_) return;
    samples_ = s;
    if (sceneFbo_) { release(); alloc(); }
}

void Post::alloc() {
    sceneTex_ = makeTex(w_, h_, sceneFormat());
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    if (samples_ > 0) {
        sceneFbo_ = makeFbo(sceneTex_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH_COMPONENT24, w_, h_);
        glGenRenderbuffers(1, &msColor_);
        glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, sceneFormat(), w_, h_);
        glGenFramebuffers(1, &msFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("msaa fbo incomplete");
    } else {
        // no multisampling: the scene draws straight into the hdr texture, depth from a plain renderbuffer
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w_, h_);
        sceneFbo_ = makeFbo(sceneTex_, msDepth_);
        msColor_ = 0; msFbo_ = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    int bw = w_ / (lite_ ? 4 : 2), bh = h_ / (lite_ ? 4 : 2);
    brightTex_ = makeTex(std::max(1, bw), std::max(1, bh), HDR);
    brightFbo_ = makeFbo(brightTex_);
    for (int i = 0; i < MIPS; i++) {
        mipW_[i] = std::max(1, bw); mipH_[i] = std::max(1, bh);
        for (int k = 0; k < 2; k++) { mipTex_[i][k] = makeTex(mipW_[i], mipH_[i], HDR); mipFbo_[i][k] = makeFbo(mipTex_[i][k]); }
        bw /= 2; bh /= 2;
    }
    duoTex_ = makeTex(w_, h_, GL_RGBA8); duoFbo_ = makeFbo(duoTex_);
    smearTex_ = 0; smearFbo_ = 0;                        // made on first use: only bleed needs it
    for (int k = 0; k < 2; k++) { afterTex_[k] = makeTex(w_, h_, GL_RGBA8); afterFbo_[k] = makeFbo(afterTex_[k]); }
    afterPrimed_ = false;
}

void Post::release() {
    if (!sceneFbo_) return;
    unsigned fbos[] = { sceneFbo_, brightFbo_, duoFbo_, afterFbo_[0], afterFbo_[1], smearFbo_ };
    unsigned texs[] = { sceneTex_, brightTex_, duoTex_, afterTex_[0], afterTex_[1], smearTex_ };
    glDeleteFramebuffers(6, fbos); glDeleteTextures(6, texs);
    for (int i = 0; i < MIPS; i++) { glDeleteFramebuffers(2, mipFbo_[i]); glDeleteTextures(2, mipTex_[i]); }
    unsigned rbs[] = { msColor_, msDepth_ };
    glDeleteRenderbuffers(2, rbs);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    sceneFbo_ = 0; msFbo_ = 0; msColor_ = 0; msDepth_ = 0;
}

void Post::drawFullscreen() { glBindVertexArray(vao_); glDrawArrays(GL_TRIANGLES, 0, 3); }

void Post::beginScene() {
    prof().begin("scene");
    glBindFramebuffer(GL_FRAMEBUFFER, samples_ > 0 ? msFbo_ : sceneFbo_);
    glViewport(0, 0, w_, h_);
    if (samples_ > 0) glEnable(GL_MULTISAMPLE); else glDisable(GL_MULTISAMPLE);
}

static void bindTex(int unit, unsigned tex) { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, tex); }

void Post::endScene(unsigned target, const PostParams& p, int outW, int outH) {
    prof().begin("resolve");
    if (samples_ > 0) {
        // resolve the multisampled scene into the single-sample HDR texture
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFbo_);
        glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);

    // ---- bloom: bright pass at half res, 5 blurred mips, composite ----
    prof().begin("bright");
    glBindFramebuffer(GL_FRAMEBUFFER, brightFbo_);
    glViewport(0, 0, mipW_[0], mipH_[0]);
    bright_->use(); bindTex(0, sceneTex_); bright_->set("tDiffuse", 0);
    bright_->set("uThreshold", p.bloomThreshold); bright_->set("uSmoothWidth", 0.01f);
    drawFullscreen();
    unsigned src = brightTex_;
    static const int RADII[MIPS] = { 3, 5, 7, 9, 11 };
    prof().begin("blur");
    blur_->use(); blur_->set("tDiffuse", 0);
    for (int i = 0; i < MIPS; i++) {
        glViewport(0, 0, mipW_[i], mipH_[i]);
        blur_->set("uRadius", RADII[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, mipFbo_[i][0]);
        bindTex(0, src); blur_->set("uDirection", glm::vec2(1.f / mipW_[i], 0)); drawFullscreen();
        glBindFramebuffer(GL_FRAMEBUFFER, mipFbo_[i][1]);
        bindTex(0, mipTex_[i][0]); blur_->set("uDirection", glm::vec2(0, 1.f / mipH_[i])); drawFullscreen();
        src = mipTex_[i][1];
    }
    // ---- bloom + duotone, one pass, 8 bit out ----
    prof().begin("grade");
    glBindFramebuffer(GL_FRAMEBUFFER, duoFbo_);
    glViewport(0, 0, w_, h_);
    grade_->use();
    bindTex(0, sceneTex_); grade_->set("tScene", 0);
    for (int i = 0; i < MIPS; i++) bindTex(1 + i, mipTex_[i][1]);
    grade_->set("tBlur0", 1); grade_->set("tBlur1", 2); grade_->set("tBlur2", 3); grade_->set("tBlur3", 4); grade_->set("tBlur4", 5);
    grade_->set("uStrength", p.bloomStrength); grade_->set("uRadius", p.bloomRadius);
    grade_->set("uColA", p.colA); grade_->set("uColB", p.colB); grade_->set("uColC", p.colC);
    grade_->set("uTritone", p.tritone ? 1 : 0);
    grade_->set("uExposure", p.exposure); grade_->set("uContrast", p.contrast); grade_->set("uSplit", p.split);
    grade_->set("uGrainT", p.grainT); grade_->set("uGrain", p.grain); grade_->set("uKick", p.kick);
    grade_->set("uTrueColor", trueColor_ ? 1 : 0);
    drawFullscreen();

    // ---- smear (bleed only; its target is made the first time it is needed) ----
    unsigned afterIn = duoTex_;
    if (p.smear > 0) {
        prof().begin("smear");
        if (!smearFbo_) { smearTex_ = makeTex(w_, h_, GL_RGBA8); smearFbo_ = makeFbo(smearTex_); }
        glBindFramebuffer(GL_FRAMEBUFFER, smearFbo_);
        smear_->use(); bindTex(0, duoTex_); smear_->set("tDiffuse", 0);
        smear_->set("uTime", p.grainT); smear_->set("uKick", p.kick); smear_->set("uAmount", p.smear);
        drawFullscreen();
        afterIn = smearTex_;
    }

    // ---- afterimage, film grain inside: ping-pong ----
    prof().begin("after");
    int cur = afterIdx_, prev = 1 - afterIdx_;
    glBindFramebuffer(GL_FRAMEBUFFER, afterFbo_[cur]);
    after_->use();
    bindTex(0, afterPrimed_ ? afterTex_[prev] : afterIn); after_->set("tOld", 0);
    bindTex(1, afterIn); after_->set("tNew", 1);
    after_->set("uDamp", p.afterDamp); after_->set("uTime", p.grainT); after_->set("uIntensity", p.filmIntensity);
    drawFullscreen();
    afterPrimed_ = true;
    afterIdx_ = prev;

    // ---- to the target. same size: a framebuffer copy. scaled (the window bigger than the stack): a textured
    // triangle, because gl refuses a scaled blit into a multisampled target and a window may be one ----
    prof().begin("copy");
    const int ow = outW > 0 ? outW : w_, oh = outH > 0 ? outH : h_;
    if (ow == w_ && oh == h_) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, afterFbo_[cur]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target);
        glBlitFramebuffer(0, 0, w_, h_, 0, 0, ow, oh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        glViewport(0, 0, ow, oh);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        glViewport(0, 0, ow, oh);
        blit_->use(); bindTex(0, afterTex_[cur]); blit_->set("tDiffuse", 0);
        drawFullscreen();
    }
    glEnable(GL_DEPTH_TEST);
    prof().frameDone();
}

} // namespace chaos
