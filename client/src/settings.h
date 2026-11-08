#pragma once
// ~/.config/chaos/settings.json. What the panel edits and what wallpaper mode reads at start.
#include <string>
#include <filesystem>

namespace chaos {

struct Persisted {
    std::string seed;               // hex of the dimension to show as wallpaper, empty = random each start
    std::string mode;               // "", "zen", "calm", "chaos", "bleed"
    std::string monitors = "mirror";// "mirror" (same dimension everywhere) or "each" (one per monitor)
    int wallpaperFps = 30;
    bool pauseOnBattery = true;
    bool pauseOnFullscreen = false;  // x11 wallpaper mode: pause while a fullscreen window has focus. off by default
    bool batteryCap = false;         // the app window: cap itself at 30 fps on battery. off by default
    bool vsync = true;
    int fpsCap = 0;
    bool poster = true;
    std::string registryUrl;        // empty = CHAOS_REGISTRY env, else https://chaos-eng.tetstack.dev
    // display: this install's choice of what to draw and what it costs. never part of the seed
    std::string layers;             // "", "nosats", "main"
    float renderScale = 1.f;        // 0.5 .. 1
    int msaa = 2;                   // 0, 2, 4
    std::string detail = "full";    // "full" or "lite" (cheaper background noise and bloom chain)
    std::string theme = "dark";     // "dark" or "light"
    std::string vaultDir;           // where the .chaos files live. empty = ~/.local/share/chaos/vault
    // your own model on the stage (offline feature, a display choice)
    bool modelOn = false;
    std::string modelPath;          // a .cheng
    std::string modelSkin = "mono"; // "berry", "mono", "wire", "own" (its own textures / colours), "random"
    std::string modelColor = "#e7eee9";
    std::string modelSkinSeed;      // hex of a seed whose look the model wears (berry skin, palette). empty = the current seed
};
int modelSkinIndex(const std::string& name, unsigned randomSalt = 0);   // "random" picks by salt
bool parseHexColor(const std::string& s, float rgb[3]);

std::filesystem::path settingsPath();
Persisted loadSettings();
bool saveSettings(const Persisted& p);

// XDG autostart: ~/.config/autostart/chaos.desktop running `chaos --wallpaper`
bool autostartEnabled();
bool setAutostart(bool on);

} // namespace chaos
