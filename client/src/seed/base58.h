#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
namespace chaos {
std::string base58Encode(const uint8_t* data, size_t len);     // bitcoin alphabet
std::vector<uint8_t> base58Decode(std::string_view s);         // throws on a bad char
}
