#include "chaos_file/format.h"
#include "server_pubkey.h"
#include "blake3.h"
#include <sodium.h>
#include <fstream>
#include <chrono>
#include <cstring>
#include <stdexcept>

namespace chaos::cf {

static void sodiumInit() { static bool done = false; if (!done) { if (sodium_init() < 0) throw std::runtime_error("sodium_init failed"); done = true; } }

const char* stateName(int s) { static const char* N[] = { "preview", "rolled", "claimed", "revoked" }; return N[s & 3]; }

uint64_t nowMs() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

void randomBytes(uint8_t* out, size_t n) { sodiumInit(); randombytes_buf(out, n); }

static void tagged(blake3_hasher& h, const char* tag) { blake3_hasher_init(&h); blake3_hasher_update(&h, tag, std::strlen(tag)); }

std::array<uint8_t, 32> commitOf(const uint8_t cr[32]) {
    blake3_hasher h; tagged(h, CHAOS_TAG_COMMIT);
    blake3_hasher_update(&h, cr, 32);
    std::array<uint8_t, 32> out; blake3_hasher_finalize(&h, out.data(), 32);
    return out;
}

Seed32 deriveSeed(const Ticket& t, const uint8_t cr[32]) {
    blake3_hasher h; tagged(h, CHAOS_TAG_SEED);
    blake3_hasher_update(&h, t.id, 16);
    blake3_hasher_update(&h, t.server_random, 32);
    blake3_hasher_update(&h, cr, 32);
    Seed32 s; blake3_hasher_finalize(&h, s.data(), 32);
    return s;
}

void putTicket(chaos_file_t& f, const Ticket& t) { std::memcpy(reinterpret_cast<uint8_t*>(&f) + CHAOS_TICKET_OFFSET, &t, sizeof t); }
Ticket getTicket(const chaos_file_t& f) { Ticket t; std::memcpy(&t, reinterpret_cast<const uint8_t*>(&f) + CHAOS_TICKET_OFFSET, sizeof t); return t; }
bool ticketIsZero(const chaos_file_t& f) { for (uint8_t b : f.ticket_id) if (b) return false; return true; }

template <class T> static bool readExact(const std::filesystem::path& p, T& out) {
    std::ifstream in(p, std::ios::binary);
    return bool(in.read(reinterpret_cast<char*>(&out), sizeof out)) && in.peek() == std::ifstream::traits_type::eof();
}
template <class T> static bool writeExact(const std::filesystem::path& p, const T& v) {
    std::ofstream out(p, std::ios::binary);
    return bool(out.write(reinterpret_cast<const char*>(&v), sizeof v));
}
bool read(const std::filesystem::path& p, chaos_file_t& out) { return readExact(p, out); }
bool write(const std::filesystem::path& p, const chaos_file_t& f) { return writeExact(p, f); }
bool readTicket(const std::filesystem::path& p, Ticket& out) { return readExact(p, out); }
bool writeTicket(const std::filesystem::path& p, const Ticket& t) { return writeExact(p, t); }
bool readReveal(const std::filesystem::path& p, uint8_t cr[32]) { std::ifstream in(p, std::ios::binary); return bool(in.read(reinterpret_cast<char*>(cr), 32)); }
bool writeReveal(const std::filesystem::path& p, const uint8_t cr[32]) { std::ofstream out(p, std::ios::binary); return bool(out.write(reinterpret_cast<const char*>(cr), 32)); }

// ---- signatures: domain tag || signed range -------------------------------------------------

static std::vector<uint8_t> ticketMessage(const Ticket& t) {
    std::vector<uint8_t> m(CHAOS_TAG_TICKET, CHAOS_TAG_TICKET + std::strlen(CHAOS_TAG_TICKET));
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&t);
    m.insert(m.end(), raw, raw + CHAOS_TICKET_SIGNED_LEN);     // everything before sig
    return m;
}
static std::vector<uint8_t> certMessage(const chaos_file_t& f) {
    std::vector<uint8_t> m(CHAOS_TAG_CERT, CHAOS_TAG_CERT + std::strlen(CHAOS_TAG_CERT));
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&f);
    m.insert(m.end(), raw + CHAOS_CERT_SIGNED_OFFSET, raw + CHAOS_CERT_SIGNED_OFFSET + CHAOS_CERT_SIGNED_LEN);
    return m;
}

bool verifyTicketSig(const Ticket& t, const uint8_t pk[32]) { sodiumInit(); auto m = ticketMessage(t); return crypto_sign_verify_detached(t.sig, m.data(), m.size(), pk) == 0; }
bool verifyCertSig(const chaos_file_t& f, const uint8_t pk[32]) { sodiumInit(); auto m = certMessage(f); return crypto_sign_verify_detached(f.cert_sig, m.data(), m.size(), pk) == 0; }

void keypair(uint8_t pk[32], uint8_t sk[64]) { sodiumInit(); crypto_sign_keypair(pk, sk); }
void signTicket(Ticket& t, const uint8_t sk[64]) { sodiumInit(); auto m = ticketMessage(t); crypto_sign_detached(t.sig, nullptr, m.data(), m.size(), sk); }
void signCert(chaos_file_t& f, const uint8_t sk[64]) { sodiumInit(); auto m = certMessage(f); crypto_sign_detached(f.cert_sig, nullptr, m.data(), m.size(), sk); }

// ---- verify -----------------------------------------------------------------------------------

Check verify(const chaos_file_t& f, const uint8_t pk[32]) {
    Check c;
    auto fail = [&](const char* w) { if (c.why.empty()) c.why = w; };
    c.layout = f.magic == CHAOS_MAGIC && f.version == CHAOS_VERSION && f.state <= CHAOS_STATE_REVOKED
            && f.family < CHAOS_FAMILY_COUNT && f.tier < CHAOS_TIER_COUNT && f.mode < CHAOS_MODE_COUNT && f.display_len <= 44;
    if (!c.layout) { fail("layout: bad magic, version or out-of-range field"); return c; }

    if (f.state == CHAOS_STATE_PREVIEW) {
        c.commit = c.seed = c.cert = true; c.ticket = ticketIsZero(f);
        if (!c.ticket) fail("preview carries a ticket");
        return c;
    }
    if (ticketIsZero(f)) { fail("no ticket"); return c; }
    Ticket t = getTicket(f);
    c.ticket = verifyTicketSig(t, pk);
    if (!c.ticket) fail("ticket signature");
    auto want = commitOf(f.client_random);
    c.commit = std::memcmp(want.data(), t.commit, 32) == 0;
    if (!c.commit) fail("client_random does not match the commit in the ticket");
    Seed32 s = deriveSeed(t, f.client_random);
    c.seed = std::memcmp(s.data(), f.seed_full, 32) == 0;
    if (!c.seed) fail("seed_full does not derive from ticket + client_random");
    if (f.state == CHAOS_STATE_ROLLED) { c.cert = true; return c; }
    c.cert = verifyCertSig(f, pk);
    if (!c.cert) fail("cert signature");
    if (f.owner_id == 0) fail("claimed with no owner");
    return c;
}

Check verify(const chaos_file_t& f) { return verify(f, CHAOS_SERVER_PUBKEY); }

} // namespace chaos::cf
