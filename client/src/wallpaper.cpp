#include "wallpaper.h"
#include "wallpaper_seed.h"
#include "model/cheng.h"
#include "platform/power.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include "platform/window.h"
#include "platform/x11_wallpaper.h"
#include "platform/wl_layer.h"
#include <chrono>
#include "render/renderer.h"
#include "scene/dimension.h"
#include "seed/seed.h"
#include "settings.h"
#include "vault/activation.h"
#include "blake3.h"

namespace chaos {

namespace {
struct Screen {
    std::unique_ptr<Window> window;
    std::unique_ptr<Dimension> dim;
    std::unique_ptr<Renderer> renderer;
    int w = 0, h = 0;
};
Seed32 perMonitorSeed(const Seed32& base, int i) {
    blake3_hasher h; blake3_hasher_init(&h);
    blake3_hasher_update(&h, base.data(), 32);
    blake3_hasher_update(&h, "monitor", 7);
    uint8_t ib = uint8_t(i); blake3_hasher_update(&h, &ib, 1);
    Seed32 s; blake3_hasher_finalize(&h, s.data(), 32); return s;
}
}

int runWallpaper(const WallpaperArgs& a) {
    Persisted cfg = loadSettings();
    std::string seedHex = !a.seedHex.empty() ? a.seedHex : cfg.seed;
    std::string modeStr = !a.mode.empty() ? a.mode : cfg.mode;
    std::string monitors = !a.monitors.empty() ? a.monitors : cfg.monitors;
    int fps = a.fps > 0 ? a.fps : std::max(1, cfg.wallpaperFps);
    std::string note;
    auto picked = pickWallpaperSeed(seedHex, a.online, note);
    if (!picked) { std::fprintf(stderr, "%s\n", note.c_str()); return 3; }
    if (!note.empty()) std::printf("%s\n", note.c_str());
    Seed32 base = *picked;
    Mode mode = Mode::Chaos; bool hasMode = !modeStr.empty() && parseMode(modeStr, mode);
    std::optional<Mesh> modelMesh; glm::vec3 modelRgb(0.9f);
    std::optional<Dimension> lookFrom;
    if (cfg.modelOn && !cfg.modelPath.empty()) {
        modelMesh = model::readCheng(cfg.modelPath); float rgb[3]; if (parseHexColor(cfg.modelColor, rgb)) modelRgb = glm::vec3(rgb[0], rgb[1], rgb[2]);
        if (!cfg.modelSkinSeed.empty()) { try { lookFrom = rollDimension(seedFromHex(cfg.modelSkinSeed)); } catch (...) {} }
    }

    // ---- backend: native wayland layer shell when the session offers it, else x11 / xwayland ----
    std::string backend = a.backend.empty() || a.backend == "auto" ? (wl::available() ? "wayland" : "x11") : a.backend;
    if (backend == "wayland") {
        wl::LayerWallpaper lw;
        std::string err;
        if (!lw.connect(err)) { std::fprintf(stderr, "wayland layer shell: %s. falling back to x11.\n", err.c_str()); backend = "x11"; }
        else {
            std::vector<std::unique_ptr<Dimension>> dims;
            std::vector<std::unique_ptr<Renderer>> rends;
            for (int k = 0; k < lw.outputCount(); k++) {
                Seed32 seed = (monitors == "each" && k > 0) ? perMonitorSeed(base, k) : base;
                const auto& o = lw.output(k);
                dims.push_back(std::make_unique<Dimension>(rollDimension(seed)));
                if (hasMode) dims.back()->setDisplayMode(mode);
                if (modelMesh) { applyModel(*dims.back(), *modelMesh, modelSkinIndex(cfg.modelSkin, unsigned(k)), modelRgb); if (lookFrom) copyLook(*dims.back(), *lookFrom); }
                std::printf("output %d %s %dx%d@%d  seed %s  %s %s  (layer shell)\n", k, o.name.c_str(), o.width, o.height, o.scale, toShortHex(seed).c_str(), shapeName(dims.back()->shapeType), modeName(dims.back()->displayMode));
            }
            // renderers need their output's context current: create them inside the first frame
            using clock = std::chrono::steady_clock;
            auto t0 = clock::now(); double lastT = 0, lastCheck = 0; bool paused = false; int frames = 0;
            while (true) {
                double now = std::chrono::duration<double>(clock::now() - t0).count();
                if (!a.noPause && now - lastCheck > 0.5) {
                    lastCheck = now;
                    bool p = cfg.pauseOnBattery && power::onBattery();     // no fullscreen probe on wayland: compositors do not expose it
                    if (p != paused) { paused = p; std::printf("%s\n", paused ? "paused (battery)" : "resumed"); }
                }
                if (paused) { lw.pump(); std::this_thread::sleep_for(std::chrono::milliseconds(250)); lastT = now; continue; }
                float dt = float(std::min(0.1, now - lastT)); lastT = now;
                bool alive = lw.frame([&](int k, int w, int h) {
                    if (rends.size() <= size_t(k)) rends.resize(k + 1);
                    if (!rends[k]) { rends[k] = std::make_unique<Renderer>(*dims[k], w, h); rends[k]->setLayers(layersFromName(cfg.layers)); rends[k]->setQuality({ cfg.renderScale, cfg.msaa, cfg.detail == "lite" }); }
                    rends[k]->resize(w, h);
                    rends[k]->frame(float(now), dt, 0);
                });
                if (!alive) { std::fprintf(stderr, "wayland display gone\n"); return 1; }
                if (a.frames > 0 && ++frames >= a.frames) return 0;
                double target = 1.0 / fps, used = std::chrono::duration<double>(clock::now() - t0).count() - now;
                if (used < target) std::this_thread::sleep_for(std::chrono::duration<double>(target - used));
            }
        }
    }

    // ---- x11 / xwayland: first window forces the platform; the rest follow
    int count = 0;
    std::vector<Screen> screens;
    {
        WindowOptions probe; probe.forceX11 = true; probe.visible = false; probe.autoShow = false; probe.width = 64; probe.height = 64;
        Window first(probe);
        if (!first.isX11()) { std::fprintf(stderr, "wallpaper mode needs x11 (or xwayland). the wayland layer shell is v1.2.\n"); return 2; }
        GLFWmonitor** mons = glfwGetMonitors(&count);
        for (int i = 0; i < count; i++) {
            int mx, my, mw, mh; glfwGetMonitorWorkarea(mons[i], &mx, &my, &mw, &mh);
            glfwGetMonitorPos(mons[i], &mx, &my);
            const GLFWvidmode* vm = glfwGetVideoMode(mons[i]);
            WindowOptions o; o.forceX11 = true; o.decorated = false; o.focusable = false; o.positioned = true;
            o.x = mx; o.y = my; o.width = vm->width; o.height = vm->height; o.autoShow = false;
            o.title = "CHAOS wallpaper " + std::to_string(i);
            Screen s; s.window = std::make_unique<Window>(o);
            x11::makeWallpaper(s.window->handle());
            s.window->show();
            Seed32 seed = (monitors == "each" && i > 0) ? perMonitorSeed(base, i) : base;
            s.window->makeCurrent();
            s.dim = std::make_unique<Dimension>(rollDimension(seed));
            if (hasMode) s.dim->setDisplayMode(mode);
            if (modelMesh) { applyModel(*s.dim, *modelMesh, modelSkinIndex(cfg.modelSkin, unsigned(i)), modelRgb); if (lookFrom) copyLook(*s.dim, *lookFrom); }
            s.window->framebufferSize(s.w, s.h);
            s.renderer = std::make_unique<Renderer>(*s.dim, s.w, s.h);
            s.renderer->setLayers(layersFromName(cfg.layers)); s.renderer->setQuality({ cfg.renderScale, cfg.msaa, cfg.detail == "lite" });
            std::printf("monitor %d %s %dx%d+%d+%d  window 0x%lx  seed %s  %s %s\n", i, glfwGetMonitorName(mons[i]), vm->width, vm->height, mx, my,
                        x11::nativeWindow(s.window->handle()), toShortHex(seed).c_str(), shapeName(s.dim->shapeType), modeName(s.dim->displayMode));
            screens.push_back(std::move(s));
        }
        // `first` dies here, glfw stays up because the screens hold windows
    }
    if (screens.empty()) { std::fprintf(stderr, "no monitors\n"); return 2; }

    double sim = 0, lastT = glfwGetTime(), lastCheck = 0; bool paused = false; int frames = 0;
    while (true) {
        screens[0].window->poll();
        bool closing = false; for (auto& s : screens) closing |= s.window->shouldClose();
        if (closing) break;
        double now = glfwGetTime();
        if (!a.noPause && now - lastCheck > 0.5) {
            lastCheck = now;
            bool p = (cfg.pauseOnFullscreen && x11::activeWindowIsFullscreen()) || (cfg.pauseOnBattery && power::onBattery());
            if (p != paused) { paused = p; std::printf("%s\n", paused ? "paused (fullscreen app or battery)" : "resumed"); }
        }
        if (paused) { std::this_thread::sleep_for(std::chrono::milliseconds(250)); lastT = glfwGetTime(); continue; }
        float dt = float(std::min(0.1, now - lastT)); lastT = now; sim += dt;
        for (auto& s : screens) {
            s.window->makeCurrent();
            int w, h; s.window->framebufferSize(w, h);
            if (w == 0 || h == 0) continue;
            s.renderer->resize(w, h);
            glfwSwapInterval(0);                    // the cap below paces us; vsync on N windows would stack
            s.renderer->frame(float(sim), dt, 0);
            s.window->swap();
        }
        if (a.frames > 0 && ++frames >= a.frames) break;
        double target = 1.0 / fps, used = glfwGetTime() - now;
        if (used < target) std::this_thread::sleep_for(std::chrono::duration<double>(target - used));
    }
    return 0;
}

} // namespace chaos
