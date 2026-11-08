#include "seed/seed.h"
#include "blake3.h"
#include "seed/base58.h"
#include "seed/bip39.h"
#include <algorithm>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstdio>

namespace chaos {

Seed32 randomSeed() {
    Seed32 s{};
    std::ifstream f("/dev/urandom", std::ios::binary);
    if (!f.read(reinterpret_cast<char*>(s.data()), s.size()))
        throw std::runtime_error("cannot read /dev/urandom");
    return s;
}

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

Seed32 seedFromHex(std::string_view hex) {
    if (hex.size() != 64) throw std::runtime_error("seed must be 64 hex chars");
    Seed32 s{};
    for (size_t i = 0; i < 32; i++) {
        int hi = hexVal(hex[2 * i]), lo = hexVal(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) throw std::runtime_error("seed is not hex");
        s[i] = uint8_t(hi * 16 + lo);
    }
    return s;
}

std::string toHex(const Seed32& s) {
    std::string out(64, '0');
    for (size_t i = 0; i < 32; i++) std::snprintf(&out[2 * i], 3, "%02x", s[i]);
    return out;
}

std::string toShortHex(const Seed32& s) { return toHex(s).substr(0, SEED_SHORT_BYTES * 2); }

Seed32 seedFromString(std::string_view in) {
    if (in.size() == 64) { bool hex = true; for (char ch : in) if (hexVal(ch) < 0) hex = false; if (hex) return seedFromHex(in); }
    if (in.find(' ') != std::string_view::npos) return bip39Decode(in);
    std::vector<uint8_t> b = base58Decode(in);
    if (b.size() != 32) throw std::runtime_error("seed must be 64 hex, 32-byte base58, or 24 words");
    Seed32 s{}; std::copy(b.begin(), b.end(), s.begin());
    return s;
}

Xoshiro256 subStream(const Seed32& seed, std::string_view label) {
    blake3_hasher h;
    blake3_hasher_init(&h);
    blake3_hasher_update(&h, seed.data(), SEED_VISUAL_BYTES);
    blake3_hasher_update(&h, label.data(), label.size());
    uint8_t out[32];
    blake3_hasher_finalize(&h, out, 32);
    return Xoshiro256::fromBytes(out);
}

} // namespace chaos
