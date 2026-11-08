/*  CHAOS .chaos file, version 3.
 *
 *  Single source of truth for the client (C++) and the registry (TypeScript port, keep in sync).
 *  512 bytes, little-endian, fixed layout. Nothing in here is secret.
 *
 *  The roll is commit-reveal, so neither side can grind rarity:
 *    1. client draws client_random (32), sends commit = blake3("CHAOS-COMMIT-v3" || client_random) with the ticket request
 *    2. registry answers with a signed ticket: ticket_id, user, issued, that commit, and server_random (32)
 *    3. seed_full = blake3("CHAOS-SEED-v3" || ticket_id || server_random || client_random)
 *  The client cannot pick client_random after seeing server_random (the commit is in the signed ticket), the
 *  registry cannot pick server_random knowing the outcome (it only knows the commit). instance_id is metadata,
 *  it is never hashed: anything the client could vary freely must stay out of the seed.
 *
 *  Signed ranges:
 *    ticket_sig = ed25519( "CHAOS-TICKET-v3" || bytes[88..184) )   ticket_id, user, issued, commit, server_random
 *    cert_sig   = ed25519( "CHAOS-CERT-v3"   || bytes[8..288)  )   seed, inputs, whole ticket, props, owner, nonce
 *  Preview rolls (no ticket) use ticket_id = 0, state = CHAOS_STATE_PREVIEW; they are never claimable.
 */
#ifndef CHAOS_FORMAT_H
#define CHAOS_FORMAT_H

#include <stdint.h>

#define CHAOS_MAGIC    0x53414843u   /* "CHAS" when read as little-endian u32 */
#define CHAOS_VERSION  3u
#define CHAOS_FILE_SIZE 512u
#define CHAOS_TICKET_WIRE_SIZE 160u  /* bytes [88..248) of the file, what POST /api/ticket returns */

#define CHAOS_TAG_COMMIT "CHAOS-COMMIT-v3"
#define CHAOS_TAG_SEED   "CHAOS-SEED-v3"
#define CHAOS_TAG_TICKET "CHAOS-TICKET-v3"
#define CHAOS_TAG_CERT   "CHAOS-CERT-v3"

enum chaos_state {
    CHAOS_STATE_PREVIEW = 0,   /* rolled offline, no ticket, never claimable */
    CHAOS_STATE_ROLLED  = 1,   /* has a ticket, not yet claimed */
    CHAOS_STATE_CLAIMED = 2,   /* cert_sig present, owned on the registry */
    CHAOS_STATE_REVOKED = 3    /* superseded by a transfer; kept as a receipt */
};

enum chaos_tier { CHAOS_TIER_COMMON = 0, CHAOS_TIER_UNCOMMON, CHAOS_TIER_RARE, CHAOS_TIER_EPIC, CHAOS_TIER_LEGENDARY, CHAOS_TIER_MYTHIC, CHAOS_TIER_COUNT };
enum chaos_mode { CHAOS_MODE_ZEN = 0, CHAOS_MODE_CALM, CHAOS_MODE_CHAOS, CHAOS_MODE_BLEED, CHAOS_MODE_COUNT };

/* the 41 generators bucketed into 16 families. the generator id itself is not stored, the seed replays it.
 *  0 knots         torus knot, lissajous knot, braid, clelia spiral
 *  1 superformula  supershape, harmonic blob, superellipsoid
 *  2 tori          wobble torus, fluted torus, torus arc
 *  3 polyhedra     icosahedron, octahedron, dodecahedron, box
 *  4 primitives    sphere, cylinder, cone, ring, plane
 *  5 lathes        lathe, lathe pod, rose lathe
 *  6 shells        spiral shell, corkscrew, conch
 *  7 sheets        saddle sheet, sinc ripple, gyroid sheet
 *  8 ribbed        ribbed cylinder, rippled sphere
 *  9 tubes         spline tube
 * 10 minimal       klein bottle, mobius band, enneper, dini, roman surface
 * 11 clusters      berry cluster, fractal flake
 * 12 colonies      voxel colony, urchin
 * 13 compound
 * 14 reserved for the mythic-only family
 * 15 reserved
 * the table itself lives in client/src/seed/rarity.cpp (familyOf) and web/src/lib/server/props.ts. */
#define CHAOS_FAMILY_COUNT 16u

#pragma pack(push, 1)
typedef struct chaos_file {
    /* header ---------------------------------------------------------- 0 */
    uint32_t magic;                 /* 0   CHAOS_MAGIC */
    uint8_t  version;               /* 4   CHAOS_VERSION */
    uint8_t  state;                 /* 5   enum chaos_state */
    uint8_t  reserved0[2];          /* 6 */
    /* seed and the client's half of it --------------------------------- 8 */
    uint8_t  seed_full[32];         /* 8   blake3, see above */
    uint8_t  instance_id[16];       /* 40  random, generated once per install. metadata, not hashed */
    uint8_t  client_random[32];     /* 56  the reveal. preview rolls: the raw seed */
    /* generation ticket, exactly as the registry issued it -------------- 88 */
    uint8_t  ticket_id[16];         /* 88  zero for preview rolls */
    uint64_t ticket_user_id;        /* 104 */
    uint64_t ticket_issued_ms;      /* 112 unix ms */
    uint8_t  ticket_commit[32];     /* 120 blake3(tag || client_random), what the client sent before the roll */
    uint8_t  ticket_server_random[32]; /* 152 the registry's half */
    uint8_t  ticket_sig[64];        /* 184 ed25519 */
    /* derived properties (registry recomputes, never trusts) ----------- 248 */
    uint8_t  family;                /* 248 0..15 */
    uint8_t  tier;                  /* 249 enum chaos_tier */
    uint8_t  mode;                  /* 250 enum chaos_mode */
    uint8_t  reserved1;             /* 251 */
    uint32_t mutation_seed;         /* 252 */
    /* ownership (filled by the registry on claim) ---------------------- 256 */
    uint64_t claimed_at_ms;         /* 256 */
    uint64_t owner_id;              /* 264 */
    uint8_t  nonce[16];             /* 272 current valid copy, rotated on transfer */
    uint8_t  cert_sig[64];          /* 288 ed25519 */
    /* presentation ---------------------------------------------------- 352 */
    uint8_t  preview_hash[32];      /* 352 blake3 of the 1920x1080 preview png */
    uint8_t  seed_display[44];      /* 384 base58 of the whole seed_full, not NUL terminated */
    uint8_t  display_len;           /* 428 */
    uint8_t  reserved2[83];         /* 429 zero, room for later fields */
} chaos_file_t;                     /* 512 */
#pragma pack(pop)

#define CHAOS_TICKET_OFFSET        88u
#define CHAOS_TICKET_SIGNED_OFFSET 88u
#define CHAOS_TICKET_SIGNED_LEN    96u
#define CHAOS_CERT_SIGNED_OFFSET   8u
#define CHAOS_CERT_SIGNED_LEN      280u

#ifdef __cplusplus
static_assert(sizeof(chaos_file_t) == CHAOS_FILE_SIZE, "chaos_file_t must be exactly 512 bytes");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(chaos_file_t) == CHAOS_FILE_SIZE, "chaos_file_t must be exactly 512 bytes");
#endif

#endif /* CHAOS_FORMAT_H */
