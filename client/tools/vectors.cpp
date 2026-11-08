// Dumps what the TypeScript server must reproduce bit for bit: props per seed, a signed ticket, a rolled file,
// a claimed file. CHAOS/test/vectors/*.json. Run from the repo root after a build.
#include "scene/props.h"
#include "chaos_file/format.h"
#include "seed/base58.h"
#include "server_pubkey.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
using namespace chaos;
static std::string hex(const uint8_t* d, size_t n) { std::string s(n * 2, '0'); for (size_t i = 0; i < n; i++) std::snprintf(&s[2 * i], 3, "%02x", d[i]); return s; }
int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : "CHAOS/test/vectors";
    std::ofstream p(out + "/props.json"); p << "[\n";
    for (int i = 0; i < 400; i++) {
        Seed32 s{}; for (int b = 0; b < 32; b++) s[b] = uint8_t((i * 2654435761u + b * 40503u + (b * i)) >> 3);
        s[31] = uint8_t(i);
        auto r = subStream(s, "rarity"); r.next();   // make sure the stream itself is checked too
        SeedProps pr = deriveProps(s);
        p << (i ? ",\n" : "") << "{\"seed\":\"" << hex(s.data(), 32) << "\",\"tier\":" << int(pr.tier) << ",\"mode\":" << int(pr.nativeMode)
          << ",\"shape\":" << pr.shapeType << ",\"family\":" << pr.family << ",\"mutation_seed\":" << pr.mutationSeed
          << ",\"base58\":\"" << base58Encode(s.data(), 32) << "\",\"rarity_u64\":\"" << std::to_string(subStream(s, "rarity").next()) << "\"}";
    }
    p << "\n]\n";
    // a ticket + rolled + claimed file signed with the dev key
    uint8_t sk[64]; std::ifstream k("server/dev/server.key", std::ios::binary); k.read(reinterpret_cast<char*>(sk), 64);
    uint8_t cr[32], inst[16]; for (int i = 0; i < 32; i++) cr[i] = uint8_t(i * 7); for (int i = 0; i < 16; i++) inst[i] = uint8_t(0x50 + i);
    cf::Ticket t{}; for (int i = 0; i < 16; i++) t.id[i] = uint8_t(0xA0 + i); t.user_id = 42; t.issued_ms = 1700000000123ULL;
    auto cm = cf::commitOf(cr); std::memcpy(t.commit, cm.data(), 32); for (int i = 0; i < 32; i++) t.server_random[i] = uint8_t(0xE0 ^ (i * 13));
    cf::signTicket(t, sk);
    chaos_file_t f{}; f.magic = CHAOS_MAGIC; f.version = CHAOS_VERSION; f.state = CHAOS_STATE_ROLLED;
    Seed32 s = cf::deriveSeed(t, cr); std::memcpy(f.seed_full, s.data(), 32); std::memcpy(f.client_random, cr, 32); std::memcpy(f.instance_id, inst, 16); cf::putTicket(f, t);
    std::string rolled = hex(reinterpret_cast<uint8_t*>(&f), 512);
    SeedProps pr = deriveProps(s);
    f.family = uint8_t(pr.family); f.tier = uint8_t(int(pr.tier)); f.mode = uint8_t(int(pr.nativeMode)); f.mutation_seed = pr.mutationSeed;
    f.owner_id = 42; f.claimed_at_ms = 1700000001000ULL; for (int i = 0; i < 16; i++) f.nonce[i] = uint8_t(0xC0 + i); f.state = CHAOS_STATE_CLAIMED;
    std::string disp = base58Encode(f.seed_full, 32); f.display_len = uint8_t(disp.size()); std::memcpy(f.seed_display, disp.data(), disp.size());
    cf::signCert(f, sk);
    std::ofstream o(out + "/files.json");
    o << "{\"pubkey\":\"" << hex(CHAOS_SERVER_PUBKEY, 32) << "\",\"ticket\":\"" << hex(reinterpret_cast<uint8_t*>(&t), 160) << "\",\"client_random\":\"" << hex(cr, 32) << "\",\"commit\":\"" << hex(cm.data(), 32) << "\",\"rolled\":\"" << rolled
      << "\",\"claimed\":\"" << hex(reinterpret_cast<uint8_t*>(&f), 512) << "\",\"seed\":\"" << hex(s.data(), 32) << "\",\"tier\":" << int(pr.tier)
      << ",\"family\":" << pr.family << ",\"mode\":" << int(pr.nativeMode) << "}\n";
    std::printf("vectors -> %s\n", out.c_str());
}
