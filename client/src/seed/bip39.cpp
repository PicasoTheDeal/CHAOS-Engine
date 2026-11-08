#include "seed/bip39.h"
#include "seed/bip39_words.h"
#include <openssl/sha.h>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cstring>
namespace chaos {
std::string bip39Encode(const std::array<uint8_t, 32>& e) {
    uint8_t hash[32]; SHA256(e.data(), e.size(), hash);
    uint8_t bits[33]; std::memcpy(bits, e.data(), 32); bits[32] = hash[0];
    std::string out;
    for (int w = 0; w < 24; w++) {
        int idx = 0;
        for (int b = 0; b < 11; b++) { int bit = w * 11 + b; idx = (idx << 1) | ((bits[bit / 8] >> (7 - bit % 8)) & 1); }
        if (w) out += ' ';
        out += BIP39_WORDS[idx];
    }
    return out;
}
std::array<uint8_t, 32> bip39Decode(std::string_view in) {
    std::istringstream ss{std::string(in)};
    std::string w; std::vector<int> idx;
    while (ss >> w) {
        int found = -1;
        for (int i = 0; i < 2048; i++) if (w == BIP39_WORDS[i]) { found = i; break; }
        if (found < 0) throw std::runtime_error("unknown bip39 word: " + w);
        idx.push_back(found);
    }
    if (idx.size() != 24) throw std::runtime_error("need 24 words");
    uint8_t bits[33] = {};
    for (int wi = 0; wi < 24; wi++)
        for (int b = 0; b < 11; b++) { int bit = wi * 11 + b; if ((idx[wi] >> (10 - b)) & 1) bits[bit / 8] |= uint8_t(1 << (7 - bit % 8)); }
    std::array<uint8_t, 32> e; std::memcpy(e.data(), bits, 32);
    uint8_t hash[32]; SHA256(e.data(), 32, hash);
    if (hash[0] != bits[32]) throw std::runtime_error("bip39 checksum mismatch");
    return e;
}
}
