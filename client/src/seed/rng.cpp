#include "seed/rng.h"
#include <cstring>
#include <cmath>

namespace chaos {

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

uint64_t SplitMix64::next() {
    uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

Xoshiro256 Xoshiro256::fromBytes(const uint8_t seed[32]) {
    Xoshiro256 r;
    for (int i = 0; i < 4; i++) {
        uint64_t v = 0;
        for (int b = 0; b < 8; b++) v |= uint64_t(seed[i * 8 + b]) << (8 * b);
        r.s[i] = v;
    }
    // an all-zero state is the one state xoshiro cannot leave
    if ((r.s[0] | r.s[1] | r.s[2] | r.s[3]) == 0) r = fromU64(0x4348414f53ULL);
    return r;
}

Xoshiro256 Xoshiro256::fromU64(uint64_t seed) {
    SplitMix64 sm(seed);
    Xoshiro256 r;
    for (auto& v : r.s) v = sm.next();
    return r;
}

uint64_t Xoshiro256::next() {
    const uint64_t result = rotl(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

double Xoshiro256::uniform() {
    return double(next() >> 11) * 0x1.0p-53;
}

float Xoshiro256::range(float lo, float hi) {
    return lo + (hi - lo) * float(uniform());
}

int Xoshiro256::rangeInt(int lo, int hi) {
    return int(std::floor(lo + (double(hi) + 1.0 - lo) * uniform()));
}

void Xoshiro256::jump() {
    static const uint64_t JUMP[] = { 0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL, 0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL };
    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (uint64_t j : JUMP)
        for (int b = 0; b < 64; b++) {
            if (j & (1ULL << b)) { s0 ^= s[0]; s1 ^= s[1]; s2 ^= s[2]; s3 ^= s[3]; }
            next();
        }
    s = { s0, s1, s2, s3 };
}

} // namespace chaos
