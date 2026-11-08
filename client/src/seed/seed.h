#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include "seed/rng.h"

namespace chaos {

using Seed32 = std::array<uint8_t, 32>;

// all 32 bytes drive the picture and are the registry identity (see shared/chaos_format.h). 2^256 dimensions,
// as the design map promises. the short hex is a display prefix like a git short hash, it does not replay anything.
constexpr size_t SEED_VISUAL_BYTES = 32;
constexpr size_t SEED_SHORT_BYTES = 8;

// 32 bytes of os entropy. v0.1 only: the real roll mixes in a server ticket (see shared/chaos_format.h).
Seed32 randomSeed();
Seed32 seedFromHex(std::string_view hex);   // 64 hex chars, throws on bad input
Seed32 seedFromString(std::string_view s);  // 64 hex, 43/44 base58, or 24 bip39 words
std::string toShortHex(const Seed32& s);    // 16-char display prefix, not replayable
std::string toHex(const Seed32& s);

// independent sub-stream for one subsystem. blake3(seed || label) -> xoshiro state.
// adding a new label later does not disturb any existing one.
Xoshiro256 subStream(const Seed32& seed, std::string_view label);

} // namespace chaos
