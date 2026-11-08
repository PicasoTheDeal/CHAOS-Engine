#include <catch2/catch_test_macros.hpp>
#include "scene/mode.h"
#include "scene/dimension.h"
#include "chaos_format.h"
using namespace chaos;

TEST_CASE("mode ids match the .chaos file enum") {
    STATIC_REQUIRE(int(Mode::Zen) == CHAOS_MODE_ZEN);
    STATIC_REQUIRE(int(Mode::Calm) == CHAOS_MODE_CALM);
    STATIC_REQUIRE(int(Mode::Chaos) == CHAOS_MODE_CHAOS);
    STATIC_REQUIRE(int(Mode::Bleed) == CHAOS_MODE_BLEED);
    STATIC_REQUIRE(MODE_COUNT == CHAOS_MODE_COUNT);
}

TEST_CASE("native mode roll is deterministic and roughly 20/35/35/10") {
    int counts[4] = {};
    for (uint64_t s = 0; s < 20000; s++) {
        auto r = Xoshiro256::fromU64(s);
        counts[int(rollMode(r))]++;
    }
    REQUIRE(counts[0] > 3400); REQUIRE(counts[0] < 4600);
    REQUIRE(counts[1] > 6300); REQUIRE(counts[1] < 7700);
    REQUIRE(counts[2] > 6300); REQUIRE(counts[2] < 7700);
    REQUIRE(counts[3] > 1500); REQUIRE(counts[3] < 2500);
    auto a = Xoshiro256::fromU64(42), b = Xoshiro256::fromU64(42);
    REQUIRE(rollMode(a) == rollMode(b));
}

TEST_CASE("parseMode accepts names and digits") {
    Mode m;
    REQUIRE(parseMode("bleed", m)); REQUIRE(m == Mode::Bleed);
    REQUIRE(parseMode("0", m)); REQUIRE(m == Mode::Zen);
    REQUIRE_FALSE(parseMode("loud", m));
}

TEST_CASE("display mode pins or caps the mood without touching the seed roll") {
    Seed32 seed{}; seed[0] = 7;
    Dimension d = rollDimension(seed, 7, 0.8f);   // mood forced high, pinned
    d.moodLocked = false;                          // let the presets act for the test
    d.setDisplayMode(Mode::Zen);   REQUIRE(d.moodTarget == 0.0f);
    d.setDisplayMode(Mode::Calm);  REQUIRE(d.moodTarget <= 0.35f);
    d.setDisplayMode(Mode::Chaos); REQUIRE(d.moodTarget == d.moodSeeded);
    d.setDisplayMode(Mode::Bleed); REQUIRE(d.moodTarget == 0.95f);
    REQUIRE(d.moodSeeded == 0.8f);
    REQUIRE_FALSE(modePreset(Mode::Zen).swarm);
    REQUIRE(modePreset(Mode::Bleed).smear > 0);
}
