#include "platform/window.h"
#include "ui/mark.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <cstdio>

namespace chaos {

int Window::count_ = 0;
bool Window::x11_ = false;

static void glfwError(int code, const char* msg) { std::fprintf(stderr, "glfw error %d: %s\n", code, msg); }

static void APIENTRY glDebug(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* msg, const void*) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    std::fprintf(stderr, "gl %s: %s\n", type == GL_DEBUG_TYPE_ERROR ? "error" : "debug", msg);
}

Window::Window(int width, int height, const std::string& title, bool visible) {
    WindowOptions o; o.width = width; o.height = height; o.title = title; o.visible = visible;
    create(o);
}

Window::Window(const WindowOptions& o) { create(o); }

void Window::create(const WindowOptions& o) {
    if (count_ == 0) {
        glfwSetErrorCallback(glfwError);
        if (o.forceX11) glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        if (o.noLibdecor) glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
        if (!glfwInit()) throw std::runtime_error("glfwInit failed");
        x11_ = glfwGetPlatform() == GLFW_PLATFORM_X11;
    }
    count_++;
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);        // the post stack does its own msaa; a multisampled window refuses scaled blits into it
    glfwWindowHint(GLFW_VISIBLE, (o.visible && o.autoShow) ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, o.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUSED, o.focusable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, o.focusable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, o.decorated ? GLFW_TRUE : GLFW_FALSE);
    if (o.positioned) { glfwWindowHint(GLFW_POSITION_X, o.x); glfwWindowHint(GLFW_POSITION_Y, o.y); }
    // the desktop file is chaos.desktop: matching ids let wayland compositors and x11 pick its icon up
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "chaos");
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "chaos");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "chaos");
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    win_ = glfwCreateWindow(o.width, o.height, o.title.c_str(), nullptr, nullptr);
    if (!win_) { if (--count_ == 0) glfwTerminate(); throw std::runtime_error("glfwCreateWindow failed (need OpenGL 4.3 core)"); }
    if (o.decorated && glfwGetPlatform() == GLFW_PLATFORM_X11) {
        // the mark as the window icon. x11 only: wayland has no such request, it takes the desktop file's icon through the app id above
        auto p16 = mark::rgba(1, 0xf0, 0xb4, 0x3c), p32 = mark::rgba(2, 0xf0, 0xb4, 0x3c), p64 = mark::rgba(4, 0xf0, 0xb4, 0x3c);
        GLFWimage icons[] = { { 16, 16, p16.data() }, { 32, 32, p32.data() }, { 64, 64, p64.data() } };
        glfwSetWindowIcon(win_, 3, icons);
    }
    glfwMakeContextCurrent(win_);
    glfwSwapInterval(1);
    if (!gladLoadGL(glfwGetProcAddress)) throw std::runtime_error("gladLoadGL failed");
    if (count_ == 1) std::fprintf(stderr, "gl %s on %s (%s)\n", glGetString(GL_VERSION), glGetString(GL_RENDERER), x11_ ? "x11" : "wayland");
    if (GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(glDebug, nullptr);
    }
}

Window::~Window() {
    if (win_) glfwDestroyWindow(win_);
    if (--count_ == 0) glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(win_); }
void Window::poll() { glfwPollEvents(); }
void Window::swap() { glfwSwapBuffers(win_); }
void Window::makeCurrent() { glfwMakeContextCurrent(win_); }
void Window::show() { glfwShowWindow(win_); }
void Window::framebufferSize(int& w, int& h) const { glfwGetFramebufferSize(win_, &w, &h); }
bool Window::isX11() const { return x11_; }

} // namespace chaos
