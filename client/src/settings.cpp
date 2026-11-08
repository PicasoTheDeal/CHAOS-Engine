#include "settings.h"
#include "vault/vault.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <unistd.h>

namespace chaos {

namespace fs = std::filesystem;
using json = nlohmann::json;

fs::path settingsPath() { return vault::configDir() / "settings.json"; }

Persisted loadSettings() {
    Persisted p;
    std::ifstream in(settingsPath());
    if (!in) return p;
    try {
        json j = json::parse(in);
        p.seed = j.value("seed", p.seed);
        p.mode = j.value("mode", p.mode);
        p.monitors = j.value("monitors", p.monitors);
        p.wallpaperFps = j.value("wallpaper_fps", p.wallpaperFps);
        p.pauseOnBattery = j.value("pause_on_battery", p.pauseOnBattery);
        p.pauseOnFullscreen = j.value("pause_on_fullscreen", p.pauseOnFullscreen);
        p.batteryCap = j.value("battery_cap", p.batteryCap);
        p.vsync = j.value("vsync", p.vsync);
        p.fpsCap = j.value("fps_cap", p.fpsCap);
        p.poster = j.value("poster", p.poster);
        p.registryUrl = j.value("registry_url", p.registryUrl);
        p.layers = j.value("layers", p.layers);
        p.renderScale = j.value("render_scale", p.renderScale);
        p.msaa = j.value("msaa", p.msaa);
        p.theme = j.value("theme", p.theme);
        p.detail = j.value("detail", p.detail);
        p.vaultDir = j.value("vault_dir", p.vaultDir);
        p.modelOn = j.value("model_on", p.modelOn);
        p.modelPath = j.value("model_path", p.modelPath);
        p.modelSkin = j.value("model_skin", p.modelSkin);
        p.modelColor = j.value("model_color", p.modelColor);
        p.modelSkinSeed = j.value("model_skin_seed", p.modelSkinSeed);
    } catch (...) {}
    return p;
}

bool saveSettings(const Persisted& p) {
    json j = { { "seed", p.seed }, { "mode", p.mode }, { "monitors", p.monitors }, { "wallpaper_fps", p.wallpaperFps },
               { "pause_on_battery", p.pauseOnBattery }, { "pause_on_fullscreen", p.pauseOnFullscreen }, { "battery_cap", p.batteryCap },
               { "vsync", p.vsync }, { "fps_cap", p.fpsCap }, { "poster", p.poster }, { "registry_url", p.registryUrl },
               { "layers", p.layers }, { "render_scale", p.renderScale }, { "msaa", p.msaa }, { "detail", p.detail }, { "theme", p.theme }, { "vault_dir", p.vaultDir },
               { "model_on", p.modelOn }, { "model_path", p.modelPath }, { "model_skin", p.modelSkin }, { "model_color", p.modelColor }, { "model_skin_seed", p.modelSkinSeed } };
    std::ofstream out(settingsPath());
    return bool(out << j.dump(2) << '\n');
}

int modelSkinIndex(const std::string& name, unsigned salt) {
    if (name == "berry") return 0;
    if (name == "wire") return 2;
    if (name == "own") return 3;
    if (name == "random") return int(salt % 3);
    return 1;
}
bool parseHexColor(const std::string& s, float rgb[3]) {
    if (s.size() != 7 || s[0] != '#') return false;
    for (int i = 0; i < 3; i++) { char* e = nullptr; long v = std::strtol(s.substr(1 + 2 * i, 2).c_str(), &e, 16); if (!e || *e) return false; rgb[i] = float(v) / 255.f; }
    return true;
}

static fs::path autostartPath() {
    const char* c = std::getenv("XDG_CONFIG_HOME");
    fs::path base = (c && *c) ? fs::path(c) : fs::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config";
    return base / "autostart" / "chaos.desktop";
}

bool autostartEnabled() { return fs::exists(autostartPath()); }

bool setAutostart(bool on) {
    auto p = autostartPath();
    if (!on) { std::error_code ec; fs::remove(p, ec); return !fs::exists(p); }
    fs::create_directories(p.parent_path());
    char exe[4096]; ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    std::string bin = n > 0 ? std::string(exe, size_t(n)) : "chaos";
    std::ofstream out(p);
    out << "[Desktop Entry]\nType=Application\nName=CHAOS\nComment=seeded dimension as your wallpaper\nExec=" << bin << " --wallpaper\n"
           "Terminal=false\nX-GNOME-Autostart-enabled=true\nOnlyShowIn=X11;XFCE;KDE;GNOME;LXQt;MATE;Cinnamon;\n";
    return bool(out);
}

} // namespace chaos
