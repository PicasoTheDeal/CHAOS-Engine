#include "render/renderer.h"
#include "shape/primitives.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace chaos {

static const glm::vec3 WHITE(1);
static const glm::vec3 FOG(0x11 / 255.f, 0x12 / 255.f, 0x11 / 255.f);

static glm::mat4 eulerXYZ(const glm::vec3& r) {
    glm::mat4 m(1);
    m = glm::rotate(m, r.x, glm::vec3(1, 0, 0));
    m = glm::rotate(m, r.y, glm::vec3(0, 1, 0));
    m = glm::rotate(m, r.z, glm::vec3(0, 0, 1));
    return m;
}

static glm::mat4 compose(const glm::vec3& pos, const glm::vec3& rot, float scale) {
    return glm::translate(glm::mat4(1), pos) * eulerXYZ(rot) * glm::scale(glm::mat4(1), glm::vec3(scale));
}

Renderer::Renderer(Dimension& d, int w, int h)
    : d_(d), post_(w, h),
      berry_("berry.vert", "berry.frag"), bg_("bg.vert", "bg.frag"), basic_("wire.vert", "basic.frag"),
      glitch_("glitch.vert", "glitch.frag"), phong_("phong.vert", "phong.frag"), own_("own.vert", "own.frag"), points_("points.vert", "points.frag"),
      main_(d.mainMesh), dome_(sphere(90.f, 64, 64)),
      particles_(d.particles.pos, d.particles.bright), partPos_(d.particles.pos) {
    for (auto& s : d.sats) sats_.push_back(std::make_unique<GLMesh>(s.mesh));
    for (auto& g : d.folds) for (auto& p : g.planes) planes_.push_back(std::make_unique<GLMesh>(planeGeo(p.w, p.h, p.ws, p.hs)));
    for (auto& b : d.debris) debris_.push_back(std::make_unique<GLMesh>(b.mesh));
    for (auto& r : d.rings) rings_.push_back(std::make_unique<GLMesh>(torusGeo(r.r, r.t, 8, r.seg)));
    camPos_ = glm::vec3(0, d.mainRadius * 0.3f, std::max(11.5f, d.mainRadius * 2.3f));
    glEnable(GL_PROGRAM_POINT_SIZE);
    post_.setTrueColor(d.modelSkin == MODEL_OWN);
    if (d.modelSkin == MODEL_OWN && d.mainMesh.material.hasTexture()) {
        const Material& mt = d.mainMesh.material;
        glGenTextures(1, &ownTex_); glBindTexture(GL_TEXTURE_2D, ownTex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mt.texW, mt.texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, mt.rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

Renderer::~Renderer() { if (ownTex_) glDeleteTextures(1, &ownTex_); }

void Renderer::resize(int w, int h) {
    winW_ = w; winH_ = h;
    post_.resize(std::max(1, int(w * quality_.scale)), std::max(1, int(h * quality_.scale)));
}
void Renderer::setQuality(const Quality& q) {
    quality_ = q;
    quality_.scale = std::clamp(quality_.scale, 0.25f, 1.f);
    post_.setSamples(q.samples);
    post_.setLite(q.lite);
    if (winW_ > 0) resize(winW_, winH_);
}
Layers layersFromName(const std::string& name) {
    Layers l;
    if (name == "nosats") l.satellites = false;
    else if (name == "main") { l.satellites = false; l.swarm = false; l.particles = false; }
    return l;
}

void Renderer::feedBerry(Shader& sh, const Skin& k, const glm::mat4& model) {
    glm::mat4 inv = glm::inverse(model);
    glm::mat3 invRot(inv);
    glm::vec3 dirs[3];
    for (int i = 0; i < 3; i++) dirs[i] = glm::normalize(invRot * d_.lightDir[i]);
    sh.set("uProj", proj_);
    sh.set("uModelView", view_ * model);
    sh.set("uCamM", glm::vec3(inv * glm::vec4(camPos_, 1)));
    sh.setArray("uLightDir", dirs, 3);
    sh.setArray("uLightCol", lightCol_, 3);
    sh.set("uAlbedo", skinA_); sh.set("uAlbedo2", skinB_);
    sh.set("uTransColor", trans_); sh.set("uBackdrop", back_); sh.set("uAmbient", amb_);
    sh.set("uChaos", k.chaos); sh.set("uNoiseScale", k.noiseScale); sh.set("uDispStrength", k.dispStrength);
    sh.set("uCellScale", k.cellScale); sh.set("uDomeR", k.domeR); sh.set("uDomeAmp", k.domeAmp); sh.set("uSeedOff", k.seedOff);
    sh.set("uTransDepth", k.transDepth); sh.set("uBead", k.bead); sh.set("uSeeThrough", k.seeThrough); sh.set("uSeedAmt", k.seedAmt);
    sh.set("uSSS", 1.f); sh.set("uRoughness", k.roughness); sh.set("uF0", k.f0); sh.set("uSpecular", k.specular);
    sh.set("uSheen", k.sheen); sh.set("uSheenColor", WHITE); sh.set("uPoreScale", k.poreScale); sh.set("uPoreStrength", k.poreStrength);
    sh.set("uBloom", k.bloom); sh.set("uBloomScale", k.bloomScale);
}

void Renderer::setPhongLights(Shader& sh, const glm::mat4& view) {
    glm::mat3 v3(view);
    glm::vec3 dirs[3], cols[3];
    for (int i = 0; i < 3; i++) { dirs[i] = glm::normalize(v3 * d_.lightDir[i]); cols[i] = WHITE * d_.lightInt[i]; }
    sh.setArray("uLightDirV", dirs, 3);
    sh.setArray("uLightCol", cols, 3);
    sh.set("uAmbient", WHITE * 0.25f);
    sh.set("uFogColor", FOG); sh.set("uFogNear", 60.f); sh.set("uFogFar", 240.f);
}

void Renderer::frame(float t, float dt, unsigned target) {
    Dimension& d = d_;
    d.update(t, dt);
    const Motion& mo = d.motion;
    float kick = d.pal.kick * d.ml(0.15f, 1.0f);
    float seedChaos = mo.chaosFactor;
    float pulse = std::sin(t * mo.pulseSpeed * 0.5f) * 0.5f + 0.5f;
    float kickScale = 1 + kick * kick * 0.12f;
    float frameScale = dt * 60.f;    // the reference stepped some things per frame at 60 fps

    int W = post_.width(), H = post_.height();
    proj_ = glm::perspective(glm::radians(60.f), float(W) / H, 0.1f, 400.f);
    view_ = glm::lookAt(camPos_, glm::vec3(0), glm::vec3(0, 1, 0));

    skinA_ = d.pal.a; skinB_ = d.pal.b;
    trans_ = glm::mix(d.pal.b, d.pal.a, 0.5f) * 0.75f;
    back_ = glm::mix(d.pal.a * 0.35f, d.pal.b, 0.15f);
    amb_ = glm::mix(d.pal.a, WHITE, 0.5f) * 0.3f;
    for (int i = 0; i < 3; i++) lightCol_[i] = glm::mix(skinB_, WHITE, 0.5f) * (d.lightInt[i] * 0.95f);

    post_.beginScene();
    glClearColor(FOG.r, FOG.g, FOG.b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);

    // ---- the main berry ----
    glm::vec3 mainPos(std::sin(t * seedChaos * 0.5f) * 0.3f + std::cos(t * 0.3f) * 0.1f,
                      std::sin(t * mo.bobSpeed) * mo.bobAmp * 0.4f + std::cos(t * 1.2f) * 0.05f,
                      std::sin(t * 0.4f) * 0.2f);
    glm::vec3 mainRot(t * mo.rotX * 0.3f + std::sin(t * 0.2f * seedChaos) * 0.15f,
                      t * mo.rotY * 0.3f + std::cos(t * 0.25f * seedChaos) * 0.15f,
                      t * mo.rotZ * 0.3f + std::sin(t * 0.15f) * 0.1f);
    glm::mat4 mainModel = compose(mainPos, mainRot, kickScale);
    if (d.modelSkin == MODEL_MONO) {
        // one colour, lit: the phong the debris uses, no emissive beyond a touch of the colour
        phong_.use(); phong_.set("uProj", proj_); setPhongLights(phong_, view_);
        phong_.set("uModelView", view_ * mainModel);
        phong_.set("uNormalMat", glm::transpose(glm::inverse(glm::mat3(view_ * mainModel))));
        phong_.set("uColor", d.modelColor); phong_.set("uEmissive", d.modelColor * (0.06f + 0.04f * pulse));
        phong_.set("uShininess", 28.f); phong_.set("uOpacity", 1.f);
        main_.draw();
    } else if (d.modelSkin == MODEL_OWN) {
        // what it came with: base colour, vertex colours, its texture, rough / metallic, its glow, under the seed's lights
        const Material& mt = d.mainMesh.material;
        own_.use(); own_.set("uProj", proj_); setPhongLights(own_, view_);
        own_.set("uModelView", view_ * mainModel);
        own_.set("uNormalMat", glm::transpose(glm::inverse(glm::mat3(view_ * mainModel))));
        own_.set("uBase", mt.base); own_.set("uEmissive", mt.emissive + mt.base * (0.02f + 0.02f * pulse));
        own_.set("uRoughness", mt.roughness); own_.set("uMetallic", mt.metallic);
        own_.set("uHasTex", ownTex_ ? 1 : 0); own_.set("uTex", 0);
        if (ownTex_) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ownTex_); }
        glVertexAttrib2f(2, 0.f, 0.f); glVertexAttrib4f(3, 1.f, 1.f, 1.f, 1.f);   // stand-ins when the mesh has no uv / colour stream
        main_.draw();
        if (ownTex_) glBindTexture(GL_TEXTURE_2D, 0);
    } else if (d.modelSkin == MODEL_WIRE) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        basic_.use(); basic_.set("uProj", proj_); basic_.set("uModelView", view_ * mainModel);
        basic_.set("uColor", d.modelColor); basic_.set("uAlpha", 0.9f);
        main_.draw();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_BLEND);
    } else {
        berry_.use();
        feedBerry(berry_, d.skin, mainModel);
        berry_.set("uTime", t); berry_.set("uPulse", pulse); berry_.set("uMutate", d.mutV); berry_.set("uLite", quality_.lite ? 1 : 0);
        main_.draw();
    }

    const ModePreset& mp = d.preset();
    const bool drawSats = mp.swarm && layers_.satellites, drawSwarm = mp.swarm && layers_.swarm;

    // ---- satellites: the opaque berries first, the backdrop between (depth tested: pixels under a berry are never
    // shaded, and the backdrop is the most expensive thing per pixel), then the blended glitch shards ----
    for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) {
        glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
        bg_.use();
        bg_.set("uProj", proj_); bg_.set("uModelView", view_ * glm::translate(glm::mat4(1), camPos_));
        bg_.set("uTime", t); bg_.set("uSeed", d.bgSeed); bg_.set("uOctaves", quality_.lite ? 3 : 5); bg_.set("uColorA", d.pal.a); bg_.set("uColorB", d.pal.b); bg_.set("uDensity", d.bgDensity);
        dome_.draw();
        glDepthMask(GL_TRUE);
    }
    for (size_t i = 0; drawSats && i < d.sats.size(); i++) {
        const Satellite& s = d.sats[i];
        if ((pass == 0) != s.berry) continue;
        float a = s.angle + t * s.orbitSpeed * 0.4f + std::sin(t * 0.1f + s.phase) * 0.2f;
        float cr = s.radius + std::sin(t * 0.5f + s.phase) * 0.4f;
        glm::vec3 pos(std::cos(a) * cr, s.heightOffset + std::sin(t * s.orbitSpeed * 0.7f + s.phase) * 0.8f + std::cos(t * 0.3f) * 0.3f, std::sin(a) * cr);
        glm::vec3 rot(t * 0.5f + s.phase + s.orbitTilt + std::sin(t + s.phase), t * 0.3f + s.phase * 0.5f + std::cos(t * 0.8f), s.orbitTilt * 0.5f + std::sin(t * 0.4f));
        float sc = (1 + 0.35f * std::sin(t * 0.8f + s.phase) * pulse) * kickScale;
        glm::mat4 model = compose(pos, rot, sc);
        if (s.berry) {
            berry_.use();
            feedBerry(berry_, s.skin, model);
            berry_.set("uTime", t); berry_.set("uPulse", pulse); berry_.set("uMutate", d.mutV * 0.6f); berry_.set("uLite", quality_.lite ? 1 : 0);
            sats_[i]->draw();
        } else {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glitch_.use();
            glitch_.set("uProj", proj_); glitch_.set("uModelView", view_ * model);
            glitch_.set("uNormalMat", glm::transpose(glm::inverse(glm::mat3(view_ * model))));
            glitch_.set("uTime", t); glitch_.set("uBaseColor", s.color); glitch_.set("uInvert", s.invert);
            glitch_.set("uBrightness", s.brightness); glitch_.set("uGlitchAmt", s.glitchAmt); glitch_.set("uSpeed", s.speed); glitch_.set("uMode", s.mode);
            sats_[i]->draw();
            glDisable(GL_BLEND);
        }
    }

    }
    // ---- transparent: rings, debris, fold planes ----
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    basic_.use(); basic_.set("uProj", proj_);
    for (size_t i = 0; drawSwarm && i < d.rings.size(); i++) {
        Ring& r = d.rings[i];
        r.rot += glm::vec3(r.rotX, r.rotY, r.rotZ) * 0.02f * frameScale;
        float a = t * r.orbitSpeed + r.phase;
        glm::vec3 pos(std::cos(a) * (r.orbitRadius + std::sin(t * 0.3f + r.phase) * 0.5f) + std::sin(t * 0.25f + r.phase) * 0.3f,
                      std::sin(t * 0.4f + r.phase) * 0.6f + std::cos(t * 0.25f + r.phase) * 0.3f,
                      std::sin(a) * (r.orbitRadius + std::cos(t * 0.3f + r.phase) * 0.5f) + std::cos(t * 0.35f + r.phase) * 0.3f);
        float sc = (1 + 0.25f * std::sin(t * 0.6f + r.phase) * pulse) * kickScale;
        basic_.set("uModelView", view_ * compose(pos, r.rot, sc));
        basic_.set("uColor", glm::vec3(r.bright * (0.7f + 0.3f * std::sin(t * 0.4f + r.phase))));
        basic_.set("uAlpha", r.opacity);
        if (r.wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        rings_[i]->draw();
        if (r.wire) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    phong_.use(); phong_.set("uProj", proj_);
    setPhongLights(phong_, view_);
    for (size_t i = 0; drawSwarm && i < d.debris.size(); i++) {
        Debris& b = d.debris[i];
        b.rot += glm::vec3(b.rotSpeed.x * (1 + pulse * 0.5f), b.rotSpeed.y * (1 + pulse * 0.5f), b.rotSpeed.z) * frameScale;
        glm::vec3 pos = b.pos + glm::vec3(std::sin(t * 0.5f + b.oscPhase) * 18 + std::cos(t * 0.3f) * 4,
                                          std::cos(t * 0.7f + b.oscPhase) * 14 + std::sin(t * 0.4f) * 4,
                                          std::sin(t * 0.9f + b.oscPhase) * 10);
        float br = std::min(0.85f, b.baseBrightness * (std::sin(t * 3.5f + b.oscPhase) * 0.45f + 0.55f)) * 0.7f;
        float sc = (1 + std::sin(t * 2.5f + b.oscPhase) * 0.4f) * kickScale;
        glm::mat4 model = compose(pos, b.rot, sc);
        phong_.set("uModelView", view_ * model);
        phong_.set("uNormalMat", glm::transpose(glm::inverse(glm::mat3(view_ * model))));
        phong_.set("uColor", glm::vec3(br));
        phong_.set("uEmissive", glm::vec3(br * (0.15f + 0.15f * std::sin(t * 0.8f + b.oscPhase))));
        phong_.set("uShininess", b.shininess); phong_.set("uOpacity", b.opacity);
        debris_[i]->draw();
    }
    size_t pi = 0;
    if (drawSwarm) for (auto& g : d.folds) {
        glm::vec3 grot(std::cos(t * 0.15f) * 0.15f, std::sin(t * 0.2f) * 0.4f + std::cos(t * 0.1f) * 0.2f, 0);
        glm::mat4 group = eulerXYZ(grot);
        for (auto& p : g.planes) {
            glm::vec3 rot(std::sin(t * 0.5f + p.foldPhase) * 1.2f + std::sin(t * 0.2f) * 0.2f, std::cos(t * 0.3f + p.foldPhase) * 0.8f, std::sin(t * 0.7f + p.foldPhase) * 0.6f);
            glm::vec3 pos = p.pos + glm::vec3(std::sin(t * 0.4f + p.foldPhase) * 10 + std::cos(t * 0.2f) * 2,
                                              std::cos(t * 0.6f + p.foldPhase) * 8 + std::sin(t * 0.3f) * 2,
                                              std::sin(t * 0.8f + p.foldPhase) * 14);
            float br = std::min(0.9f, p.baseBrightness * (std::sin(t * 2.5f + p.foldPhase) * 0.35f + 0.65f));
            glm::mat4 model = group * compose(pos, rot, 1);
            phong_.set("uModelView", view_ * model);
            phong_.set("uNormalMat", glm::transpose(glm::inverse(glm::mat3(view_ * model))));
            phong_.set("uColor", glm::vec3(br));
            phong_.set("uEmissive", glm::vec3(br * (0.12f + 0.12f * std::sin(t * 0.7f + p.foldPhase))));
            phong_.set("uShininess", p.shininess); phong_.set("uOpacity", p.opacity);
            planes_[pi++]->draw();
        }
    }

    // ---- wire ghost (not on a model: applyModel sets wireAlpha to 0) ----
    glDepthMask(GL_FALSE);
    if (d.wireAlpha > 0.f) {
        float wireScale = 1.01f * (1 + 0.03f * std::sin(t * 3.0f)) * kickScale;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        basic_.use();
        basic_.set("uModelView", view_ * compose(mainPos, mainRot, wireScale));
        basic_.set("uColor", WHITE); basic_.set("uAlpha", d.wireAlpha);
        main_.draw();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // ---- particles: additive, no depth write ----
    if (layers_.particles && !partPos_.empty()) {
        float step = 0.0008f * seedChaos * frameScale, ca = std::cos(step), sa = std::sin(step), spread = d.particles.spread;
        for (size_t i = 0; i < partPos_.size(); i++) {
            glm::vec3& p = partPos_[i];
            float px = p.x, pz = p.z;
            p.x = px * ca - pz * sa; p.z = px * sa + pz * ca;
            p.x += std::sin(t * 0.15f + i * 0.001f) * 0.0008f * frameScale;
            p.y += std::cos(t * 0.12f + i * 0.0013f) * 0.0008f * frameScale;
            p.z += std::sin(t * 0.14f + i * 0.0017f) * 0.0008f * frameScale;
            float r = glm::length(p);
            if (r > spread * 1.9f || r < spread * 0.2f) p *= 0.97f;
        }
        particles_.updatePositions(partPos_);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        points_.use();
        points_.set("uProj", proj_);
        points_.set("uModelView", view_ * eulerXYZ(glm::vec3(std::sin(t * 0.01f + seedChaos) * 0.12f, t * 0.02f, 0)));
        points_.set("uSize", 0.045f); points_.set("uViewportH", float(H)); points_.set("uOpacity", d.particles.opacity);
        particles_.draw();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ---- post ----
    PostParams pp;
    pp.bloomStrength = (0.18f + 0.25f * (0.5f + 0.5f * std::sin(t * 0.08f + seedChaos)) * pulse) * d.ml(0.75f, 1.0f) + kick * 0.6f;
    pp.colA = d.pal.a; pp.colB = d.pal.b; pp.colC = d.palC; pp.tritone = d.unlocks.thirdStop;
    pp.exposure = d.duoExposure; pp.contrast = d.duoContrast; pp.split = d.duoSplit;
    pp.grainT = t; pp.grain = d.ml(0.004f, 0.012f); pp.kick = kick;
    pp.filmIntensity = d.ml(0.03f, 0.14f);
    pp.afterDamp = std::min(0.97f, (mp.afterDamp >= 0 ? mp.afterDamp : d.ml(0.80f, 0.94f)) + d.unlocks.afterDampBonus);
    pp.smear = mp.smear;
    post_.endScene(target, pp, winW_, winH_);
}

} // namespace chaos
