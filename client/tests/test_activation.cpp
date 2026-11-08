#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include "vault/activation.h"
using namespace chaos;

TEST_CASE("activation list round trips and survives a broken file") {
    auto tmp = std::filesystem::temp_directory_path() / "chaos-activation-test";
    std::filesystem::remove_all(tmp);
    setenv("XDG_DATA_HOME", (tmp / "data").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (tmp / "config").c_str(), 1);
    Seed32 s{}; s[0] = 5; Seed32 t{}; t[0] = 6;
    REQUIRE_FALSE(activation::isActivated(s));
    activation::record(s, { 123, "7", "owned", true, 500, "ab" });
    REQUIRE(activation::isActivated(s));
    REQUIRE_FALSE(activation::isActivated(t));
    auto e = activation::get(s);
    REQUIRE(e); REQUIRE(e->owner == "7"); REQUIRE(e->valueCents == 500); REQUIRE(e->current);
    activation::record(s, { 124, "9", "transferred", false, 500, "cd" });   // a later check updates in place
    REQUIRE(activation::load().size() == 1);
    REQUIRE_FALSE(activation::get(s)->current);
    std::ofstream(activation::path()) << "{ nope";
    REQUIRE(activation::load().empty());
    std::filesystem::remove_all(tmp);
}
