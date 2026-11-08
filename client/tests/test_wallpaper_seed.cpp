#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include "wallpaper_seed.h"
#include "seed/seed.h"

using namespace chaos;

TEST_CASE("wallpaper seed rule: offline with nothing activated plays nothing, and says why") {
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "chaos-wseed-test";
    std::filesystem::remove_all(tmp);
    setenv("XDG_DATA_HOME", tmp.c_str(), 1);     // an empty vault, no activated.json
    std::string note;
    REQUIRE_FALSE(pickWallpaperSeed("", false, note).has_value());
    REQUIRE(note.find("vault is empty") != std::string::npos);
    REQUIRE_FALSE(pickWallpaperSeed("zz", true, note).has_value());
    REQUIRE(note.rfind("bad seed", 0) == 0);
    REQUIRE(pickWallpaperSeed("", true, note).has_value());
    auto s = randomSeed();
    REQUIRE_FALSE(pickWallpaperSeed(toHex(s), false, note).has_value());
    REQUIRE(pickWallpaperSeed(toHex(s), true, note) == s);
}

TEST_CASE("the lobby is one fixed seed") {
    REQUIRE(lobbySeed() == lobbySeed());
    REQUIRE(toHex(lobbySeed()).size() == 64);
    REQUIRE(toHex(lobbySeed()) == "b409a3aa376e9aae5bafdcf22d20bd2870c2a84dc06ca16fb867313d49b41b6a");   // pinned: the lobby must never drift
}
