#pragma once
// X11 only: turn a glfw window into a desktop-layer wallpaper and read the things that decide when to pause.
// Works on real X11 and on xwayland (where the compositor decides how much of the hint it honours).
struct GLFWwindow;
namespace chaos::x11 {
// _NET_WM_WINDOW_TYPE_DESKTOP, _NET_WM_STATE below/sticky/skip taskbar/skip pager, WM_HINTS input off,
// empty XShape input region. Call before show().
bool makeWallpaper(GLFWwindow* win);
unsigned long nativeWindow(GLFWwindow* win);
bool activeWindowIsFullscreen();    // _NET_ACTIVE_WINDOW has _NET_WM_STATE_FULLSCREEN
}
