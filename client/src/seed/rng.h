#pragma once
#include <cstdint>
#include <array>
#include <string_view>

namespace chaos {

// SplitMix64: used only to expand a 64-bit value into a 256-bit xoshiro state.
struct SplitMix64 {
    uint64_t s;
    explicit SplitMix64(uint64_t seed) : s(seed) {}
    uint64_t next();
};

// xoshiro256**: the stream every generator reads from.
// Blackman & Vigna, "Scrambled Linear Pseudorandom Number Generators", 2021.
struct Xoshiro256 {
    std::array<uint64_t, 4> s{};

    static Xoshiro256 fromBytes(const uint8_t seed[32]);   // 32 bytes straight into the state
    static Xoshiro256 fromU64(uint64_t seed);               // expanded through SplitMix64

    uint64_t next();
    double   uniform();                      // [0, 1)
    float    range(float lo, float hi);      // [lo, hi)
    int      rangeInt(int lo, int hi);       // [lo, hi] inclusive, like the reference randInt
    bool     chance(double p) { return uniform() < p; }
    void     jump();                         // 2^128 steps, for independent streams
};

} // namespace chaos
