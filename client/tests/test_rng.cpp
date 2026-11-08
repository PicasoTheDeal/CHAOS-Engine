#include <catch2/catch_test_macros.hpp>
#include "seed/rng.h"
#include "seed/seed.h"
using namespace chaos;

TEST_CASE("splitmix64 matches the reference sequence for seed 0") {
    // first outputs of SplitMix64(0), as published by Vigna
    SplitMix64 sm(0);
    REQUIRE(sm.next() == 0xe220a8397b1dcdafULL);
    REQUIRE(sm.next() == 0x6e789e6aa1b965f4ULL);
    REQUIRE(sm.next() == 0x06c45d188009454fULL);
}

TEST_CASE("xoshiro256** is deterministic and streams are independent") {
    Seed32 seed{};
    for (int i = 0; i < 32; i++) seed[i] = uint8_t(i * 7 + 3);
    auto a = subStream(seed, "shape"), b = subStream(seed, "shape"), c = subStream(seed, "skin");
    REQUIRE(a.next() == b.next());
    REQUIRE(a.next() != c.next());
    for (int i = 0; i < 1000; i++) {
        double u = a.uniform();
        REQUIRE(u >= 0.0);
        REQUIRE(u < 1.0);
        int k = a.rangeInt(3, 7);
        REQUIRE(k >= 3);
        REQUIRE(k <= 7);
    }
}

TEST_CASE("hex round trip") {
    Seed32 s = randomSeed();
    REQUIRE(seedFromHex(toHex(s)) == s);
    REQUIRE_THROWS(seedFromHex("zz"));
    // every byte matters: flipping the last one changes the picture
    Seed32 s2 = s; s2[31] ^= 1;
    REQUIRE(subStream(s2, "shape").next() != subStream(s, "shape").next());
    REQUIRE(toShortHex(s).size() == 16);
}
