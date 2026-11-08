#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <cstring>
#include "vault/vault.h"
#include "seed/base58.h"
using namespace chaos;

TEST_CASE("vault keeps a preview file that round trips") {
    auto tmp = std::filesystem::temp_directory_path() / "chaos-vault-test";
    std::filesystem::remove_all(tmp);
    setenv("XDG_DATA_HOME", (tmp / "data").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (tmp / "config").c_str(), 1);

    Seed32 seed{}; for (int i = 0; i < 32; i++) seed[i] = uint8_t(200 - i);
    Dimension d = rollDimension(seed, 7);
    std::vector<uint8_t> png = { 1, 2, 3, 4 };
    auto e = vault::keep(d, png);
    REQUIRE(std::filesystem::file_size(e.chaosPath) == 512);
    REQUIRE(std::filesystem::exists(e.pngPath));

    auto entries = vault::list();
    REQUIRE(entries.size() == 1);
    const chaos_file_t& f = entries[0].file;
    REQUIRE(f.state == CHAOS_STATE_PREVIEW);
    REQUIRE(std::memcmp(f.seed_full, seed.data(), 32) == 0);
    REQUIRE(f.family == familyOf(7));
    REQUIRE(f.tier == uint8_t(int(d.tier)));
    REQUIRE(std::string(reinterpret_cast<const char*>(f.seed_display), f.display_len) == base58Encode(seed.data(), 32));
    REQUIRE(vault::instanceId() == vault::instanceId());
    REQUIRE(std::memcmp(f.instance_id, vault::instanceId().data(), 16) == 0);
    std::filesystem::remove_all(tmp);
}
