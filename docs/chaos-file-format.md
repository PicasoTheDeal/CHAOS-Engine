# .chaos file format (v3)

A `.chaos` file is the claim ticket for one seed. It is 512 bytes, binary, little-endian.
The layout is `shared/chaos_format.h`. Both client and server parse exactly that struct. `seed_display` holds the
base58 of the whole 32-byte seed (43 or 44 chars). Family ids are the 16 buckets listed in the header comment.

The file is not the asset. The registry (website) is the source of truth. The file is what you
upload to prove a roll happened, and what the server hands back as a signed receipt.

## Trust model

- The server owns one ed25519 keypair. The client ships only the public key.
- The client has no secret. Anything the client could sign, anyone could forge, so the client signs nothing.
- Two signatures, both made by the server:
  - `ticket_sig` is issued **before** the roll. It is the server saying "this user may roll once".
    The weekly allowance is enforced when tickets are issued.
  - `cert_sig` is issued **on claim**. It is the server saying "this seed is owned by this account, this copy is current".

## Seed derivation (commit-reveal)

```
commit     = blake3("CHAOS-COMMIT-v3" || client_random)          sent by the client with the ticket request
ticket     = { id, user, issued, commit, server_random } + sig    server_random drawn after the server saw the commit
seed_full  = blake3("CHAOS-SEED-v3" || ticket_id || server_random || client_random)
```

Neither side can steer the roll: the client cannot change `client_random` after seeing `server_random` because the
commit is inside the signed ticket, and the server cannot choose `server_random` for an outcome because it does not
know `client_random` yet. `instance_id` is metadata and is never hashed. On claim the server checks the ticket
signature, the commit, the derivation, then derives rarity, family and mode itself. It never trusts what the client
wrote.

## States

| state   | meaning |
|---------|---------|
| preview | rolled offline, no ticket, renders fine, can never be claimed |
| rolled  | has a valid ticket, not uploaded yet |
| claimed | `cert_sig` valid, owned on the registry |
| revoked | nonce superseded by a transfer, kept as a receipt |

## Signed ranges

| signature  | domain tag          | bytes      | covers |
|------------|---------------------|------------|--------|
| ticket_sig | `CHAOS-TICKET-v3`   | 88..184    | ticket_id, user, issued, commit, server_random |
| cert_sig   | `CHAOS-CERT-v3`     | 8..288     | seed, inputs, whole ticket, props, owner, nonce |

The domain tag is prepended to the message before signing so a ticket signature can never be replayed as a cert.

## Transfer

The server rotates `nonce`, re-signs, writes a new file for the new owner and marks the old nonce revoked in its
database. The old file still verifies offline (the signature is real) but the registry rejects it. Offline
verification therefore answers "was this ever real", not "is this current". Only the registry answers the second.

## Ticket wire format

160 bytes: `id[16] user_id[u64] issued_ms[u64] commit[32] server_random[32] sig[64]`. The signature covers
`"CHAOS-TICKET-v3" || first 96 bytes`. The client stores the whole ticket in the file at offset 88.

## Reference implementation

`client/src/chaos_file/format.cpp` (parse, derive, verify, sign) and `client/tools/registry_dev.cpp` (the registry's
claim and transfer rules). The registry (its own private repo) has the TypeScript port, `format.ts` and `registry.ts`, checked against the
vectors `client/build/chaos-vectors` generates. The dev key pair in `server/dev/` is for tests
and local play only.
