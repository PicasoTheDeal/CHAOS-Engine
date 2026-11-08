#pragma once
// .chaos v3: read, write, commit, derive, verify, and (registry side) sign. the layout is shared/chaos_format.h.
// used by the client, the dev registry tool and the tests. the TypeScript registry mirrors it.
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include "chaos_format.h"
#include "seed/seed.h"

namespace chaos::cf {

// A generation ticket as the registry hands it out: 160 bytes on the wire, the file's bytes [88..248).
#pragma pack(push, 1)
struct Ticket {
    uint8_t  id[16];
    uint64_t user_id;
    uint64_t issued_ms;
    uint8_t  commit[32];          // blake3(CHAOS_TAG_COMMIT || client_random), sent by the client before the roll
    uint8_t  server_random[32];
    uint8_t  sig[64];
};
#pragma pack(pop)
static_assert(sizeof(Ticket) == CHAOS_TICKET_WIRE_SIZE);

const char* stateName(int state);
uint64_t nowMs();
void randomBytes(uint8_t* out, size_t n);

// the client's half: draw client_random, publish its commit
std::array<uint8_t, 32> commitOf(const uint8_t clientRandom[32]);
// seed_full = blake3(tag || ticket_id || server_random || client_random). instance_id is deliberately absent.
Seed32 deriveSeed(const Ticket& t, const uint8_t clientRandom[32]);

void putTicket(chaos_file_t& f, const Ticket& t);
Ticket getTicket(const chaos_file_t& f);
bool ticketIsZero(const chaos_file_t& f);

// io
bool read(const std::filesystem::path& p, chaos_file_t& out);
bool write(const std::filesystem::path& p, const chaos_file_t& f);
bool readTicket(const std::filesystem::path& p, Ticket& out);
bool writeTicket(const std::filesystem::path& p, const Ticket& t);
bool readReveal(const std::filesystem::path& p, uint8_t clientRandom[32]);     // the 32 bytes the client committed to
bool writeReveal(const std::filesystem::path& p, const uint8_t clientRandom[32]);

struct Check {
    bool layout = false;   // magic, version, ranges
    bool commit = false;   // client_random matches the commit inside the signed ticket (n/a for preview, reported true)
    bool seed = false;     // seed_full recomputes from the ticket + client_random
    bool ticket = false;   // ticket signature
    bool cert = false;     // cert signature
    std::string why;       // first failure, empty when everything the state needs passed
    bool ok() const { return why.empty(); }
};
Check verify(const chaos_file_t& f, const uint8_t pubkey[32]);
Check verify(const chaos_file_t& f);                         // CHAOS_SERVER_PUBKEY
bool verifyTicketSig(const Ticket& t, const uint8_t pubkey[32]);
bool verifyCertSig(const chaos_file_t& f, const uint8_t pubkey[32]);

// signing: registry side only. sk is a libsodium 64-byte secret key.
void keypair(uint8_t pk[32], uint8_t sk[64]);
void signTicket(Ticket& t, const uint8_t sk[64]);
void signCert(chaos_file_t& f, const uint8_t sk[64]);

} // namespace chaos::cf
