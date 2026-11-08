#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include "settings.h"
using namespace chaos;

TEST_CASE("settings.json round trips and autostart writes a desktop entry") {
    auto tmp = std::filesystem::temp_directory_path() / "chaos-settings-test";
    std::filesystem::remove_all(tmp);
    setenv("XDG_CONFIG_HOME", (tmp / "config").c_str(), 1);
    setenv("XDG_DATA_HOME", (tmp / "data").c_str(), 1);
    Persisted d = loadSettings();                 // no file: defaults
    REQUIRE(d.monitors == "mirror"); REQUIRE(d.wallpaperFps == 30); REQUIRE(d.seed.empty());
    Persisted p; p.seed = std::string(64, 'a'); p.mode = "bleed"; p.monitors = "each"; p.wallpaperFps = 12; p.pauseOnBattery = false; p.fpsCap = 45;
    REQUIRE(saveSettings(p));
    Persisted q = loadSettings();
    REQUIRE(q.seed == p.seed); REQUIRE(q.mode == "bleed"); REQUIRE(q.monitors == "each"); REQUIRE(q.wallpaperFps == 12);
    REQUIRE_FALSE(q.pauseOnBattery); REQUIRE(q.fpsCap == 45); REQUIRE_FALSE(q.pauseOnFullscreen); REQUIRE_FALSE(q.batteryCap);   // both pauses are opt-in
    // a broken file falls back to defaults instead of crashing
    std::ofstream(settingsPath()) << "{ not json";
    REQUIRE(loadSettings().monitors == "mirror");
    REQUIRE_FALSE(autostartEnabled());
    REQUIRE(setAutostart(true));
    REQUIRE(autostartEnabled());
    std::ifstream de(tmp / "config" / "autostart" / "chaos.desktop"); std::string all((std::istreambuf_iterator<char>(de)), {});
    REQUIRE(all.find("--wallpaper") != std::string::npos);
    REQUIRE(setAutostart(false));
    REQUIRE_FALSE(autostartEnabled());
    std::filesystem::remove_all(tmp);
}
