#pragma once
// BIP-0039 mnemonic for the 32-byte seed: 256 bits of entropy + 8-bit sha256 checksum = 24 words.
#include <string>
#include <string_view>
#include <array>
#include <cstdint>
namespace chaos {
std::string bip39Encode(const std::array<uint8_t, 32>& entropy);
std::array<uint8_t, 32> bip39Decode(std::string_view words);   // throws on unknown word or bad checksum
}
