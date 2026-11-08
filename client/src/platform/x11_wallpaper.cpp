#include "platform/x11_wallpaper.h"
#include "platform/window.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <string>
#include <cstring>

namespace chaos::x11 {

unsigned long nativeWindow(GLFWwindow* win) { return Window::isX11Platform() ? glfwGetX11Window(win) : 0; }

bool makeWallpaper(GLFWwindow* win) {
    if (!Window::isX11Platform()) return false;
    Display* dpy = glfwGetX11Display();
    ::Window w = glfwGetX11Window(win);
    auto atom = [&](const char* n) { return XInternAtom(dpy, n, False); };

    Atom type = atom("_NET_WM_WINDOW_TYPE"), desktop = atom("_NET_WM_WINDOW_TYPE_DESKTOP");
    XChangeProperty(dpy, w, type, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&desktop), 1);

    Atom states[] = { atom("_NET_WM_STATE_BELOW"), atom("_NET_WM_STATE_STICKY"), atom("_NET_WM_STATE_SKIP_TASKBAR"), atom("_NET_WM_STATE_SKIP_PAGER") };
    XChangeProperty(dpy, w, atom("_NET_WM_STATE"), XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(states), 4);

    long allDesktops = 0xFFFFFFFF;
    XChangeProperty(dpy, w, atom("_NET_WM_DESKTOP"), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&allDesktops), 1);

    XWMHints* hints = XAllocWMHints();
    hints->flags = InputHint; hints->input = False;
    XSetWMHints(dpy, w, hints);
    XFree(hints);

    // no input region at all: clicks fall through to whatever the wm puts behind (usually nothing, it is the desktop)
    XShapeCombineRectangles(dpy, w, ShapeInput, 0, 0, nullptr, 0, ShapeSet, Unsorted);
    XFlush(dpy);
    return true;
}

bool activeWindowIsFullscreen() {
    if (!Window::isX11Platform()) return false;
    Display* dpy = glfwGetX11Display();
    ::Window root = DefaultRootWindow(dpy);
    Atom actualType; int fmt; unsigned long n, after; unsigned char* data = nullptr;
    Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    if (XGetWindowProperty(dpy, root, active, 0, 1, False, XA_WINDOW, &actualType, &fmt, &n, &after, &data) != Success || !data) return false;
    ::Window aw = *reinterpret_cast<::Window*>(data); XFree(data); data = nullptr;
    if (!aw) return false;
    Atom state = XInternAtom(dpy, "_NET_WM_STATE", False), fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    bool full = false;
    if (XGetWindowProperty(dpy, aw, state, 0, 64, False, XA_ATOM, &actualType, &fmt, &n, &after, &data) == Success && data) {
        Atom* atoms = reinterpret_cast<Atom*>(data);
        for (unsigned long i = 0; i < n; i++) if (atoms[i] == fs) full = true;
        XFree(data);
    }
    return full;
}

} // namespace chaos::x11
