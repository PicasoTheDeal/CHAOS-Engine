#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include "chaos_file/format.h"
#include "scene/props.h"
#include "scene/dimension.h"
#include "vault/vault.h"
using namespace chaos;

static cf::Ticket makeTicket(const uint8_t cr[32], uint64_t user, const uint8_t sk[64]) {
    cf::Ticket t{}; cf::randomBytes(t.id, 16); t.user_id = user; t.issued_ms = 1700000000000ULL;
    auto c = cf::commitOf(cr); std::memcpy(t.commit, c.data(), 32); cf::randomBytes(t.server_random, 32);
    cf::signTicket(t, sk);
    return t;
}
static chaos_file_t rolledFile(const cf::Ticket& t, const uint8_t cr[32], const uint8_t inst[16]) {
    chaos_file_t f{};
    f.magic = CHAOS_MAGIC; f.version = CHAOS_VERSION; f.state = CHAOS_STATE_ROLLED;
    Seed32 s = cf::deriveSeed(t, cr);
    std::memcpy(f.seed_full, s.data(), 32);
    std::memcpy(f.client_random, cr, 32);
    std::memcpy(f.instance_id, inst, 16);
    cf::putTicket(f, t);
    return f;
}

TEST_CASE("ticket -> roll -> claim -> transfer, with tampering caught at every step") {
    uint8_t pk[32], sk[64]; cf::keypair(pk, sk);
    uint8_t wrongPk[32], wrongSk[64]; cf::keypair(wrongPk, wrongSk);

    uint8_t cr[32], inst[16]; cf::randomBytes(cr, 32); cf::randomBytes(inst, 16);
    cf::Ticket t = makeTicket(cr, 7, sk);
    REQUIRE(cf::verifyTicketSig(t, pk));
    REQUIRE_FALSE(cf::verifyTicketSig(t, wrongPk));
    cf::Ticket t2 = t; t2.user_id = 8;
    REQUIRE_FALSE(cf::verifyTicketSig(t2, pk));
    cf::Ticket t3 = t; t3.server_random[0] ^= 1;          // the registry's half is under the signature too
    REQUIRE_FALSE(cf::verifyTicketSig(t3, pk));

    chaos_file_t f = rolledFile(t, cr, inst);
    auto c = cf::verify(f, pk);
    REQUIRE(c.ok()); REQUIRE(c.layout); REQUIRE(c.ticket); REQUIRE(c.commit); REQUIRE(c.seed); REQUIRE(c.cert);

    // the client cannot pick its seed
    chaos_file_t grind = f; grind.seed_full[0] ^= 1;
    REQUIRE_FALSE(cf::verify(grind, pk).ok());
    // the client cannot swap its random after seeing the ticket: the commit pins it
    chaos_file_t swapCr = f; swapCr.client_random[0] ^= 1;
    Seed32 s2 = cf::deriveSeed(t, swapCr.client_random); std::memcpy(swapCr.seed_full, s2.data(), 32);
    auto cc = cf::verify(swapCr, pk);
    REQUIRE_FALSE(cc.ok()); REQUIRE_FALSE(cc.commit);
    // instance_id is metadata: changing it changes nothing about the seed
    chaos_file_t inst2 = f; inst2.instance_id[0] ^= 1;
    REQUIRE(cf::verify(inst2, pk).ok());
    // a preview cannot smuggle a ticket, a rolled file cannot lack one
    chaos_file_t pv = f; pv.state = CHAOS_STATE_PREVIEW;
    REQUIRE_FALSE(cf::verify(pv, pk).ok());
    chaos_file_t nt = f; std::memset(nt.ticket_id, 0, 16);
    REQUIRE_FALSE(cf::verify(nt, pk).ok());

    // claim: server derives props, fills owner/nonce, signs
    Seed32 s; std::memcpy(s.data(), f.seed_full, 32);
    SeedProps p = deriveProps(s);
    f.family = uint8_t(p.family); f.tier = uint8_t(int(p.tier)); f.mode = uint8_t(int(p.nativeMode));
    f.owner_id = 7; f.claimed_at_ms = cf::nowMs(); cf::randomBytes(f.nonce, 16);
    f.state = CHAOS_STATE_CLAIMED;
    cf::signCert(f, sk);
    c = cf::verify(f, pk);
    REQUIRE(c.ok()); REQUIRE(c.cert);
    REQUIRE_FALSE(cf::verify(f, wrongPk).ok());

    // any signed byte flipped kills the cert; a byte outside the range (state) does not
    for (size_t off : { size_t(8), size_t(100), size_t(130), size_t(160), size_t(249), size_t(264), size_t(275) }) {
        chaos_file_t x = f; reinterpret_cast<uint8_t*>(&x)[off] ^= 1;
        REQUIRE_FALSE(cf::verifyCertSig(x, pk));
    }
    chaos_file_t rv = f; rv.state = CHAOS_STATE_REVOKED;
    REQUIRE(cf::verifyCertSig(rv, pk));
    // a ticket signature cannot be replayed as a cert (different domain tag, different range)
    chaos_file_t swap = f; std::memcpy(swap.cert_sig, swap.ticket_sig, 64);
    REQUIRE_FALSE(cf::verifyCertSig(swap, pk));

    // transfer: new nonce, new owner, old copy differs
    chaos_file_t old = f;
    f.owner_id = 9; cf::randomBytes(f.nonce, 16); cf::signCert(f, sk);
    REQUIRE(cf::verify(f, pk).ok());
    REQUIRE(cf::verify(old, pk).ok());                 // still a real signature: offline says "was real", only the registry says "current"
    REQUIRE(std::memcmp(old.nonce, f.nonce, 16) != 0);
}

TEST_CASE("deriveProps agrees with rollDimension") {
    for (uint64_t k = 1; k < 6; k++) {
        Seed32 s{}; s[0] = uint8_t(k * 37); s[31] = uint8_t(k);
        SeedProps p = deriveProps(s);
        Dimension d = rollDimension(s, -1);
        REQUIRE(int(p.tier) == int(d.tier));
        REQUIRE(int(p.nativeMode) == int(d.nativeMode));
        REQUIRE(p.shapeType == d.shapeType);
        REQUIRE(p.family == d.family);
    }
}

TEST_CASE("vault writes a rolled file that verifies") {
    auto tmp = std::filesystem::temp_directory_path() / "chaos-vault-test2";
    std::filesystem::remove_all(tmp);
    setenv("XDG_DATA_HOME", (tmp / "data").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (tmp / "config").c_str(), 1);
    uint8_t pk[32], sk[64]; cf::keypair(pk, sk);
    uint8_t cr[32]; cf::randomBytes(cr, 32);
    cf::Ticket t = makeTicket(cr, 3, sk);
    Seed32 s = cf::deriveSeed(t, cr);
    Dimension d = rollDimension(s, 7);
    chaos_file_t f = vault::makeFile(d, {}, &t, cr);
    REQUIRE(f.state == CHAOS_STATE_ROLLED);
    REQUIRE(cf::verify(f, pk).ok());
    auto e = vault::keep(d, {}, &t, cr);
    chaos_file_t back{};
    REQUIRE(cf::read(e.chaosPath, back));
    REQUIRE(std::memcmp(&back, &f, 512) == 0);
    std::filesystem::remove_all(tmp);
}
