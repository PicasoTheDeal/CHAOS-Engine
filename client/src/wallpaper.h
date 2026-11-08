#pragma once
// --wallpaper: one desktop-layer window per monitor, no panel, no input, fps capped, pauses when it should.
#include <string>
#include <optional>
#include "seed/seed.h"
#include "wallpaper_seed.h"      // pickWallpaperSeed, lobbySeed
namespace chaos {
struct WallpaperArgs {
    std::string seedHex;        // empty: settings, then random
    std::string mode;           // empty: settings, then native
    std::string monitors;       // empty: settings
    int fps = 0;                // 0: settings
    int frames = 0;             // > 0: exit after this many frames (testing)
    bool noPause = false;
    bool online = true;         // false: only seeds activated on this install may play
    std::string backend;        // "", "auto", "wayland", "x11"
};
int runWallpaper(const WallpaperArgs& a);
}
