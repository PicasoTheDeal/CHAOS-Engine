/* The registry's ed25519 public key. Nothing secret here.
 * PRODUCTION KEY (2026-11-06): the matching private key lives only in the owner's safe place and as the
 * CHAOS_SERVER_KEY secret on cloudflare. the dev key for local work is server/dev/server.key; a client built with
 * this header refuses the dev registry and the other way round (see `chaos --registry`). regenerate with
 * `chaos-registry-dev keygen --key <file> --pub shared/server_pubkey.h` and rebuild the client. */
#ifndef CHAOS_SERVER_PUBKEY_H
#define CHAOS_SERVER_PUBKEY_H
#include <stdint.h>
static const uint8_t CHAOS_SERVER_PUBKEY[32] = {
    0x50, 0x9b, 0x26, 0x54, 0x15, 0x6f, 0x4c, 0x3f,
    0x32, 0xf0, 0xcf, 0xcf, 0x7d, 0x0b, 0x80, 0x3a,
    0x08, 0x3d, 0xdb, 0x6e, 0x12, 0xc1, 0x10, 0x8c,
    0x61, 0x7f, 0x17, 0x48, 0x37, 0xf6, 0xd4, 0x84
};
#endif
