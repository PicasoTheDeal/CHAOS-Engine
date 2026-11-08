#include <catch2/catch_test_macros.hpp>
#include "seed/base58.h"
#include "seed/bip39.h"
#include "seed/seed.h"
#include "seed/rarity.h"
#include "chaos_format.h"
using namespace chaos;

TEST_CASE("base58 round trip and leading zeros") {
    Seed32 z{};
    REQUIRE(base58Encode(z.data(), 32) == std::string(32, '1'));
    Seed32 s = randomSeed();
    std::string b = base58Encode(s.data(), 32);
    REQUIRE(b.size() >= 43); REQUIRE(b.size() <= 44);
    auto back = base58Decode(b);
    REQUIRE(back.size() == 32);
    REQUIRE(std::equal(back.begin(), back.end(), s.begin()));
    REQUIRE(seedFromString(b) == s);
    REQUIRE_THROWS(base58Decode("0OIl"));
}

TEST_CASE("bip39 matches the reference vectors") {
    std::array<uint8_t, 32> z{};
    std::string zw = bip39Encode(z);
    std::string expectZ; for (int i = 0; i < 23; i++) expectZ += "abandon "; expectZ += "art";
    REQUIRE(zw == expectZ);
    std::array<uint8_t, 32> e; e.fill(0x7f);
    REQUIRE(bip39Encode(e) == "legal winner thank year wave sausage worth useful legal winner thank year wave sausage worth useful legal winner thank year wave sausage worth title");
    REQUIRE(bip39Decode(bip39Encode(e)) == e);
    Seed32 s = randomSeed();
    REQUIRE(seedFromString(bip39Encode(s)) == s);
    REQUIRE_THROWS(bip39Decode("abandon abandon art"));
}

TEST_CASE("tier ids match the file enum and odds are what the map says") {
    STATIC_REQUIRE(int(Tier::Mythic) == CHAOS_TIER_MYTHIC);
    STATIC_REQUIRE(TIER_COUNT == CHAOS_TIER_COUNT);
    int counts[6] = {};
    const int N = 200000;
    for (uint64_t s = 0; s < N; s++) { auto r = Xoshiro256::fromU64(s); counts[int(rollTier(r))]++; }
    REQUIRE(counts[0] > N * 0.58); REQUIRE(counts[0] < N * 0.62);
    REQUIRE(counts[3] > N * 0.03); REQUIRE(counts[3] < N * 0.05);
    REQUIRE(counts[5] > N * 0.0005); REQUIRE(counts[5] < N * 0.0016);
}

TEST_CASE("every generator has a family and unlocks grow with tier") {
    for (int s = 0; s < 41; s++) { REQUIRE(familyOf(s) >= 0); REQUIRE(familyOf(s) < 14); }
    REQUIRE(familyOf(40) == 13);
    REQUIRE_FALSE(unlocksFor(Tier::Common).secondBerry);
    REQUIRE(unlocksFor(Tier::Rare).secondBerry);
    REQUIRE(unlocksFor(Tier::Epic).forceCompound);
    REQUIRE(unlocksFor(Tier::Legendary).forceBleed);
    REQUIRE(unlocksFor(Tier::Mythic).gold);
    REQUIRE(rarityScore(Tier::Mythic, 3, 2, 0.5f) == 5.0f);
    REQUIRE(rarityScore(Tier::Common, 10, 14, 1.0f) < 1.0f);
}
