#pragma once
#include <string>
struct GLFWwindow;

namespace chaos {

struct WindowOptions {
    int width = 1280, height = 720;
    std::string title = "CHAOS";
    bool visible = true;
    bool decorated = true;
    bool focusable = true;      // false: never takes focus (wallpaper)
    bool positioned = false;    // use x, y
    int x = 0, y = 0;
    bool forceX11 = false;      // GLFW_PLATFORM_X11, so a wayland session goes through xwayland (wallpaper mode)
    bool noLibdecor = false;    // wayland: skip libdecor (its gtk plugin can hang in containers; kwin decorates server side anyway)
    bool autoShow = true;       // false: create hidden, caller calls show() after setting native hints
};

// GLFW window with an OpenGL 4.3 core context. glfw is initialised once and torn down with the last window.
class Window {
public:
    explicit Window(const WindowOptions& o);
    Window(int width, int height, const std::string& title, bool visible = true);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void poll();                 // process events for every window
    void swap();
    void makeCurrent();
    void show();
    void framebufferSize(int& w, int& h) const;
    GLFWwindow* handle() const { return win_; }
    bool isX11() const;
    static bool isX11Platform() { return x11_; }   // after the first window: which platform glfw picked (3.3 builds are x11 only)

private:
    void create(const WindowOptions& o);
    GLFWwindow* win_ = nullptr;
    static int count_;
    static bool x11_;
};

} // namespace chaos
