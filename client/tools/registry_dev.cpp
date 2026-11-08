// chaos-registry-dev: the registry's half of the .chaos flow, locally, until the website exists.
// This is the reference the TypeScript server mirrors. One flat text db, no users, no money.
//
//   keygen  --key server.key --pub shared/server_pubkey.h
//   ticket  --key server.key --db registry.db --user N --commit <hex from chaos --commit> --out t.ticket
//   claim   --key server.key --db registry.db --in rolled.chaos --out claimed.chaos [--owner N]
//   transfer --key server.key --db registry.db --in claimed.chaos --to N --out new.chaos
//   verify  --db registry.db --in any.chaos
//
// db lines:  T <ticket_id hex> <user> <used 0|1>
//            S <seed hex> <owner> <nonce hex> <state>
#include "chaos_file/format.h"
#include "scene/props.h"
#include "seed/base58.h"
#include "server_pubkey.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

using namespace chaos;

namespace {

std::string hex(const uint8_t* d, size_t n) { std::string s(n * 2, '0'); for (size_t i = 0; i < n; i++) std::snprintf(&s[2 * i], 3, "%02x", d[i]); return s; }

struct Db {
    std::string path;
    std::map<std::string, std::pair<uint64_t, bool>> tickets;                       // id -> user, used
    struct SeedRow { uint64_t owner; std::string nonce; std::string state; };
    std::map<std::string, SeedRow> seeds;
    void load() {
        std::ifstream in(path); std::string line;
        while (std::getline(in, line)) {
            std::istringstream ss(line); char k; ss >> k;
            if (k == 'T') { std::string id; uint64_t u; int used; ss >> id >> u >> used; tickets[id] = { u, used != 0 }; }
            else if (k == 'S') { std::string seed; SeedRow r; ss >> seed >> r.owner >> r.nonce >> r.state; seeds[seed] = r; }
        }
    }
    void save() {
        std::ofstream out(path);
        for (auto& [id, t] : tickets) out << "T " << id << ' ' << t.first << ' ' << (t.second ? 1 : 0) << '\n';
        for (auto& [s, r] : seeds) out << "S " << s << ' ' << r.owner << ' ' << r.nonce << ' ' << r.state << '\n';
    }
};

bool loadKey(const std::string& p, uint8_t sk[64]) { std::ifstream in(p, std::ios::binary); return bool(in.read(reinterpret_cast<char*>(sk), 64)); }

int fail(const char* why) { std::fprintf(stderr, "refused: %s\n", why); return 2; }

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: chaos-registry-dev keygen|ticket|claim|transfer|verify [--key k] [--db d] [--in f] [--out f] [--user N] [--owner N] [--to N] [--pub h]\n"); return 1; }
    std::string cmd = argv[1], key = "server/dev/server.key", db = "server/dev/registry.db", in, out, pub, commitHex;
    uint64_t user = 1, owner = 0, to = 0;
    for (int i = 2; i < argc; i++) {
        auto arg = [&](const char* n, std::string& v) { if (!std::strcmp(argv[i], n) && i + 1 < argc) { v = argv[++i]; return true; } return false; };
        auto num = [&](const char* n, uint64_t& v) { if (!std::strcmp(argv[i], n) && i + 1 < argc) { v = std::strtoull(argv[++i], nullptr, 10); return true; } return false; };
        if (arg("--key", key) || arg("--db", db) || arg("--in", in) || arg("--out", out) || arg("--pub", pub) || arg("--commit", commitHex) || num("--user", user) || num("--owner", owner) || num("--to", to)) continue;
        std::fprintf(stderr, "unknown arg %s\n", argv[i]); return 1;
    }

    if (cmd == "keygen") {
        uint8_t pk[32], sk[64]; cf::keypair(pk, sk);
        std::ofstream(key, std::ios::binary).write(reinterpret_cast<char*>(sk), 64);
        std::printf("private key -> %s\npublic key  = { ", key.c_str());
        for (int i = 0; i < 32; i++) std::printf("0x%02x%s", pk[i], i < 31 ? ", " : " }\n");
        if (!pub.empty()) {
            std::ofstream h(pub);
            h << "/* The registry's ed25519 public key. Nothing secret here. written by chaos-registry-dev keygen. */\n#ifndef CHAOS_SERVER_PUBKEY_H\n#define CHAOS_SERVER_PUBKEY_H\n#include <stdint.h>\nstatic const uint8_t CHAOS_SERVER_PUBKEY[32] = {\n";
            for (int i = 0; i < 32; i++) { if (i % 8 == 0) h << "    "; char b[8]; std::snprintf(b, sizeof b, "0x%02x", pk[i]); h << b << (i == 31 ? "\n" : (i % 8 == 7 ? ",\n" : ", ")); }
            h << "};\n#endif\n";
            std::printf("public key  -> %s (rebuild the client)\n", pub.c_str());
        } else std::printf("paste the public key into shared/server_pubkey.h (or pass --pub shared/server_pubkey.h) and rebuild the client.\n");
        return 0;
    }

    Db d; d.path = db; d.load();

    if (cmd == "ticket") {
        uint8_t sk[64]; if (!loadKey(key, sk)) return fail("cannot read key");
        if (commitHex.size() != 64) return fail("--commit <64 hex> is the client's commitment (chaos --commit out.reveal prints it)");
        cf::Ticket t{};
        cf::randomBytes(t.id, 16);
        t.user_id = user;
        t.issued_ms = cf::nowMs();
        for (int i = 0; i < 32; i++) t.commit[i] = uint8_t(std::stoul(commitHex.substr(2 * i, 2), nullptr, 16));
        cf::randomBytes(t.server_random, 32);      // the registry's half, drawn after the client committed
        cf::signTicket(t, sk);
        if (out.empty()) out = "t.ticket";
        if (!cf::writeTicket(out, t)) return fail("cannot write ticket");
        d.tickets[hex(t.id, 16)] = { user, false };
        d.save();
        std::printf("ticket %s for user %llu -> %s\n", hex(t.id, 16).c_str(), (unsigned long long)user, out.c_str());
        return 0;
    }

    chaos_file_t f{};
    if (in.empty() || !cf::read(in, f)) return fail("need --in with a 512 byte .chaos");

    if (cmd == "verify") {
        auto c = cf::verify(f, CHAOS_SERVER_PUBKEY);
        std::string seedHex = hex(f.seed_full, 32);
        std::printf("state %s  layout %s ticket %s commit %s seed %s cert %s  -> %s %s\n", cf::stateName(f.state), c.layout ? "ok" : "BAD", c.ticket ? "ok" : "BAD", c.commit ? "ok" : "BAD", c.seed ? "ok" : "BAD", c.cert ? "ok" : "BAD", c.ok() ? "VALID" : "INVALID", c.why.c_str());
        auto it = d.seeds.find(seedHex);
        if (it == d.seeds.end()) std::printf("registry: unknown seed\n");
        else std::printf("registry: owner %llu, state %s, this copy is %s\n", (unsigned long long)it->second.owner, it->second.state.c_str(), it->second.nonce == hex(f.nonce, 16) ? "CURRENT" : "STALE (nonce superseded)");
        return c.ok() ? 0 : 2;
    }

    uint8_t sk[64]; if (!loadKey(key, sk)) return fail("cannot read key");

    if (cmd == "claim") {
        if (f.state != CHAOS_STATE_ROLLED) return fail("only a rolled file can be claimed (preview rolls have no ticket)");
        auto c = cf::verify(f, CHAOS_SERVER_PUBKEY);
        if (!c.ok()) return fail(c.why.c_str());
        std::string tid = hex(f.ticket_id, 16), seedHex = hex(f.seed_full, 32);
        auto t = d.tickets.find(tid);
        if (t == d.tickets.end()) return fail("ticket was never issued by this registry");
        if (t->second.second) return fail("ticket already used");
        if (owner == 0) owner = t->second.first;
        if (t->second.first != owner) return fail("ticket belongs to another user");
        if (d.seeds.count(seedHex)) return fail("seed already claimed");
        // never trust what the client wrote: derive the properties again
        Seed32 s; std::memcpy(s.data(), f.seed_full, 32);
        SeedProps p = deriveProps(s);
        f.family = uint8_t(p.family); f.tier = uint8_t(int(p.tier)); f.mode = uint8_t(int(p.nativeMode)); f.mutation_seed = p.mutationSeed;
        f.owner_id = owner;
        f.claimed_at_ms = cf::nowMs();
        cf::randomBytes(f.nonce, 16);
        f.state = CHAOS_STATE_CLAIMED;
        cf::signCert(f, sk);
        if (out.empty()) out = in;
        if (!cf::write(out, f)) return fail("cannot write");
        t->second.second = true;
        d.seeds[seedHex] = { owner, hex(f.nonce, 16), "owned" };
        d.save();
        std::printf("claimed %s by %llu  tier %s family %d mode %d  -> %s\n", base58Encode(f.seed_full, 32).c_str(), (unsigned long long)owner, tierName(p.tier), p.family, int(p.nativeMode), out.c_str());
        return 0;
    }

    if (cmd == "transfer") {
        if (to == 0) return fail("--to N");
        if (f.state != CHAOS_STATE_CLAIMED) return fail("only a claimed file transfers");
        auto c = cf::verify(f, CHAOS_SERVER_PUBKEY);
        if (!c.ok()) return fail(c.why.c_str());
        std::string seedHex = hex(f.seed_full, 32);
        auto it = d.seeds.find(seedHex);
        if (it == d.seeds.end()) return fail("unknown seed");
        if (it->second.owner != f.owner_id) return fail("file owner is not the registry owner");
        if (it->second.nonce != hex(f.nonce, 16)) return fail("stale copy: nonce superseded");
        f.owner_id = to;
        cf::randomBytes(f.nonce, 16);
        f.claimed_at_ms = cf::nowMs();
        cf::signCert(f, sk);
        if (out.empty()) out = in;
        if (!cf::write(out, f)) return fail("cannot write");
        it->second = { to, hex(f.nonce, 16), "owned" };
        d.save();
        std::printf("transferred to %llu, old copy revoked -> %s\n", (unsigned long long)to, out.c_str());
        return 0;
    }

    std::fprintf(stderr, "unknown command %s\n", cmd.c_str());
    return 1;
}
