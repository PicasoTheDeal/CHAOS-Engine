#include "platform/wl_layer.h"
#include <glad/gl.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
// the generated header names a parameter `namespace`, which c++ refuses
#define namespace namespace_
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#undef namespace
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <poll.h>
#include <unistd.h>

namespace chaos::wl {

namespace {

struct Screen {
    wl_output* output = nullptr;
    uint32_t name = 0;
    Output info;
    wl_surface* surface = nullptr;
    zwlr_layer_surface_v1* layer = nullptr;
    wl_egl_window* eglWindow = nullptr;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    int w = 0, h = 0;          // from configure, surface units
    bool configured = false;
};

} // namespace

struct LayerWallpaper::Impl {
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    zwlr_layer_shell_v1* shell = nullptr;
    std::vector<std::unique_ptr<Screen>> screens;
    EGLDisplay egl = EGL_NO_DISPLAY;
    EGLConfig config = nullptr;
    EGLContext context = EGL_NO_CONTEXT;
    bool gladDone = false;

    // ---- wl_output ----
    static void outGeometry(void* d, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char* make, const char* model, int32_t) {
        auto* s = static_cast<Screen*>(d); if (s->info.name.empty()) s->info.name = std::string(make) + " " + model;
    }
    static void outMode(void* d, wl_output*, uint32_t flags, int32_t w, int32_t h, int32_t) {
        auto* s = static_cast<Screen*>(d); if (flags & WL_OUTPUT_MODE_CURRENT) { s->info.width = w; s->info.height = h; }
    }
    static void outDone(void*, wl_output*) {}
    static void outScale(void* d, wl_output*, int32_t f) { static_cast<Screen*>(d)->info.scale = f; }
    static void outName(void* d, wl_output*, const char* n) { static_cast<Screen*>(d)->info.name = n; }
    static void outDesc(void*, wl_output*, const char*) {}
    static constexpr wl_output_listener outputListener = { outGeometry, outMode, outDone, outScale, outName, outDesc };

    // ---- registry ----
    static void global(void* d, wl_registry* r, uint32_t name, const char* iface, uint32_t version) {
        auto* self = static_cast<Impl*>(d);
        if (!std::strcmp(iface, wl_compositor_interface.name)) self->compositor = static_cast<wl_compositor*>(wl_registry_bind(r, name, &wl_compositor_interface, std::min(version, 4u)));
        else if (!std::strcmp(iface, zwlr_layer_shell_v1_interface.name)) self->shell = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(r, name, &zwlr_layer_shell_v1_interface, std::min(version, 4u)));
        else if (!std::strcmp(iface, wl_output_interface.name)) {
            auto s = std::make_unique<Screen>();
            s->name = name;
            s->output = static_cast<wl_output*>(wl_registry_bind(r, name, &wl_output_interface, std::min(version, 4u)));
            wl_output_add_listener(s->output, &outputListener, s.get());
            self->screens.push_back(std::move(s));
        }
    }
    static void globalRemove(void*, wl_registry*, uint32_t) {}
    static constexpr wl_registry_listener registryListener = { global, globalRemove };

    // ---- layer surface ----
    static void configure(void* d, zwlr_layer_surface_v1* ls, uint32_t serial, uint32_t w, uint32_t h) {
        auto* s = static_cast<Screen*>(d);
        zwlr_layer_surface_v1_ack_configure(ls, serial);
        s->w = int(w); s->h = int(h);
        if (s->eglWindow) wl_egl_window_resize(s->eglWindow, s->w * s->info.scale, s->h * s->info.scale, 0, 0);
        s->configured = true;
    }
    static void closed(void* d, zwlr_layer_surface_v1*) { static_cast<Screen*>(d)->configured = false; }
    static constexpr zwlr_layer_surface_v1_listener layerListener = { configure, closed };
};

bool available() {
    const char* w = std::getenv("WAYLAND_DISPLAY");
    if (!w || !*w) return false;
    wl_display* d = wl_display_connect(nullptr);
    if (!d) return false;
    bool found = false;
    struct Probe { bool* found; };
    Probe probe{ &found };
    wl_registry* reg = wl_display_get_registry(d);
    static const wl_registry_listener l = {
        [](void* data, wl_registry*, uint32_t, const char* iface, uint32_t) { if (!std::strcmp(iface, "zwlr_layer_shell_v1")) *static_cast<Probe*>(data)->found = true; },
        [](void*, wl_registry*, uint32_t) {} };
    wl_registry_add_listener(reg, &l, &probe);
    wl_display_roundtrip(d);
    wl_registry_destroy(reg);
    wl_display_disconnect(d);
    return found;
}

LayerWallpaper::LayerWallpaper() : p_(std::make_unique<Impl>()) {}

LayerWallpaper::~LayerWallpaper() {
    Impl& i = *p_;
    for (auto& s : i.screens) {
        if (s->eglSurface != EGL_NO_SURFACE) eglDestroySurface(i.egl, s->eglSurface);
        if (s->eglWindow) wl_egl_window_destroy(s->eglWindow);
        if (s->layer) zwlr_layer_surface_v1_destroy(s->layer);
        if (s->surface) wl_surface_destroy(s->surface);
        if (s->output) wl_output_destroy(s->output);
    }
    if (i.context != EGL_NO_CONTEXT) eglDestroyContext(i.egl, i.context);
    if (i.egl != EGL_NO_DISPLAY) eglTerminate(i.egl);
    if (i.shell) zwlr_layer_shell_v1_destroy(i.shell);
    if (i.compositor) wl_compositor_destroy(i.compositor);
    if (i.registry) wl_registry_destroy(i.registry);
    if (i.display) wl_display_disconnect(i.display);
}

bool LayerWallpaper::connect(std::string& err) {
    Impl& i = *p_;
    i.display = wl_display_connect(nullptr);
    if (!i.display) { err = "no wayland display"; return false; }
    i.registry = wl_display_get_registry(i.display);
    wl_registry_add_listener(i.registry, &Impl::registryListener, &i);
    wl_display_roundtrip(i.display);      // globals
    wl_display_roundtrip(i.display);      // output modes
    if (!i.compositor) { err = "no wl_compositor"; return false; }
    if (!i.shell) { err = "the compositor does not offer zwlr_layer_shell_v1 (gnome does not; kde, sway, hyprland, river do)"; return false; }
    if (i.screens.empty()) { err = "no outputs"; return false; }

    // egl on the wayland display, one gl 4.3 core context shared by every surface
    i.egl = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(i.display));
    EGLint maj, min;
    if (i.egl == EGL_NO_DISPLAY || !eglInitialize(i.egl, &maj, &min)) { err = "eglInitialize failed"; return false; }
    if (!eglBindAPI(EGL_OPENGL_API)) { err = "eglBindAPI(OPENGL) failed"; return false; }
    const EGLint cfgAttrs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE };
    EGLint n = 0;
    if (!eglChooseConfig(i.egl, cfgAttrs, &i.config, 1, &n) || n < 1) { err = "no egl config"; return false; }
    const EGLint ctxAttrs[] = { EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
    i.context = eglCreateContext(i.egl, i.config, EGL_NO_CONTEXT, ctxAttrs);
    if (i.context == EGL_NO_CONTEXT) { err = "eglCreateContext (gl 4.3 core) failed"; return false; }

    for (auto& s : i.screens) {
        s->surface = wl_compositor_create_surface(i.compositor);
        s->layer = zwlr_layer_shell_v1_get_layer_surface(i.shell, s->surface, s->output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "chaos");
        zwlr_layer_surface_v1_add_listener(s->layer, &Impl::layerListener, s.get());
        zwlr_layer_surface_v1_set_anchor(s->layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_exclusive_zone(s->layer, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(s->layer, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
        zwlr_layer_surface_v1_set_size(s->layer, 0, 0);     // the compositor sizes it to the output
        wl_surface_commit(s->surface);
    }
    // wait for the first configure of each surface
    for (int tries = 0; tries < 50; tries++) {
        bool all = true; for (auto& s : i.screens) all &= s->configured;
        if (all) break;
        if (wl_display_roundtrip(i.display) < 0) { err = "display error while configuring"; return false; }
    }
    for (auto& s : i.screens) {
        if (!s->configured || s->w == 0 || s->h == 0) { err = "layer surface never configured for " + s->info.name; return false; }
        if (s->info.scale > 1) wl_surface_set_buffer_scale(s->surface, s->info.scale);
        s->eglWindow = wl_egl_window_create(s->surface, s->w * s->info.scale, s->h * s->info.scale);
        s->eglSurface = eglCreateWindowSurface(i.egl, i.config, reinterpret_cast<EGLNativeWindowType>(s->eglWindow), nullptr);
        if (s->eglSurface == EGL_NO_SURFACE) { err = "eglCreateWindowSurface failed"; return false; }
    }
    auto& first = *i.screens.front();
    if (!eglMakeCurrent(i.egl, first.eglSurface, first.eglSurface, i.context)) { err = "eglMakeCurrent failed"; return false; }
    if (!i.gladDone) {
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(eglGetProcAddress))) { err = "gladLoadGL via egl failed"; return false; }
        i.gladDone = true;
        std::fprintf(stderr, "gl %s on %s (wayland layer shell, egl %d.%d)\n", glGetString(GL_VERSION), glGetString(GL_RENDERER), maj, min);
    }
    eglSwapInterval(i.egl, 0);           // we pace ourselves, like the x11 path
    return true;
}

int LayerWallpaper::outputCount() const { return int(p_->screens.size()); }
const Output& LayerWallpaper::output(int k) const { return p_->screens[k]->info; }

void LayerWallpaper::pump() {
    Impl& i = *p_;
    while (wl_display_prepare_read(i.display) != 0) wl_display_dispatch_pending(i.display);
    wl_display_flush(i.display);
    pollfd pfd{ wl_display_get_fd(i.display), POLLIN, 0 };
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) wl_display_read_events(i.display);
    else wl_display_cancel_read(i.display);
    wl_display_dispatch_pending(i.display);
}

bool LayerWallpaper::frame(const std::function<void(int, int, int)>& draw) {
    Impl& i = *p_;
    pump();
    if (wl_display_get_error(i.display)) return false;
    for (size_t k = 0; k < i.screens.size(); k++) {
        auto& s = *i.screens[k];
        if (!s.configured) continue;
        eglMakeCurrent(i.egl, s.eglSurface, s.eglSurface, i.context);
        draw(int(k), s.w * s.info.scale, s.h * s.info.scale);
        eglSwapBuffers(i.egl, s.eglSurface);
    }
    wl_display_flush(i.display);
    return true;
}

} // namespace chaos::wl
