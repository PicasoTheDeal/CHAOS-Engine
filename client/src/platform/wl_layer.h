#pragma once
// wayland, natively: one zwlr_layer_shell_v1 background surface per output, an EGL context each, no glfw.
// this is what makes wallpaper mode a real wallpaper on a wayland session instead of an xwayland window.
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace chaos { class Renderer; class Dimension; }

namespace chaos::wl {

bool available();   // WAYLAND_DISPLAY set and the compositor offers the layer shell

struct Output { std::string name; int width = 0, height = 0, scale = 1; };

class LayerWallpaper {
public:
    LayerWallpaper();
    ~LayerWallpaper();
    bool connect(std::string& err);                 // registry, outputs, layer surfaces, egl. false with a reason.
    int outputCount() const;
    const Output& output(int i) const;
    // per output, after the first configure: make the context current, size known. the callback owns rendering.
    // returns false when the display went away.
    bool frame(const std::function<void(int index, int width, int height)>& draw);
    void pump();                                     // read + dispatch events without blocking
private:
    struct Impl; std::unique_ptr<Impl> p_;
};

} // namespace chaos::wl
