# .chaos file format (v3)

a `.chaos` is the claim ticket for one seed. 512 bytes, binary, little endian. the layout is `shared/chaos_format.h` and both the client and the registry parse exactly that struct. `seed_display` holds the base58 of the whole 32 byte seed (43 or 44 chars). family ids aer the 16 buckets listed in the header comment.

the file is not the asset. the registry is. the file is what you upload to prove a roll happened, and what the registry hands back as a signed receipt.

## trust model

- the registry owns one ed25519 keypair. the client ships only the public key.
- the client has no secret. anything the client could sign anyone could forge, so the client signs nothing.
- 2 signatures, both made by the registry:
  - `ticket_sig` is issued before the roll. the registry saying "this user may roll once". the weekly allowance is enforced when tickets get issued.
  - `cert_sig` is issued on claim. the registry saying "this seed is owned by this account and this copy is current".

## seed derivation (commit reveal)

```
commit     = blake3("CHAOS-COMMIT-v3" || client_random)          sent by the client with the ticket request
ticket     = { id, user, issued, commit, server_random } + sig    server_random drawn after the registry saw the commit
seed_full  = blake3("CHAOS-SEED-v3" || ticket_id || server_random || client_random)
```

neither side can steer the roll. the client can't change `client_random` after seeing `server_random` cuz the commit is inside the signed ticket, and the registry can't pick `server_random` for an outcome cuz it doesn't know `client_random` yet. `instance_id` is metadata and never hashed. on claim the registry checks the ticket signature, the commit, the derivation, then derives rarity, family and mode itself. it never trusts what the client wrote.

## states

| state   | meaning |
|---------|---------|
| preview | rolled with no ticket, renders fine, can never be claimed |
| rolled  | has a valid ticket, not uploaded yet |
| claimed | `cert_sig` valid, owned on the registry |
| revoked | nonce superseded by a transfer, kept as a receipt |

## signed ranges

| signature  | domain tag          | bytes      | covers |
|------------|---------------------|------------|--------|
| ticket_sig | `CHAOS-TICKET-v3`   | 88..184    | ticket_id, user, issued, commit, server_random |
| cert_sig   | `CHAOS-CERT-v3`     | 8..288     | seed, inputs, whole ticket, props, owner, nonce |

the domain tag goes in front of the message before signing so a ticket signature can never be replayed as a cert.

## transfer

the registry rotates `nonce`, re-signs, writes a new file for the new owner and marks the old nonce revoked in its db. the old file still verifies offline (the signature is real) but the registry rejects it. so offline verification answers "was this ever real", not "is this current". only the registry answers the second one.

## ticket wire format

160 bytes: `id[16] user_id[u64] issued_ms[u64] commit[32] server_random[32] sig[64]`. the signature covers `"CHAOS-TICKET-v3" || first 96 bytes`. the client stores the whole ticket in the file at offset 88.

## reference implementation

`client/src/chaos_file/format.cpp` (parse, derive, verify, sign) and `client/tools/registry_dev.cpp` (the registry's claim and transfer rules, the c++ version the typescript was ported from). the registry (its own private repo) has `format.ts` and `registry.ts`, checked against the vectors `client/build/chaos-vectors` generates. the dev key in `server/dev/` is for tests and local play only and isn't committed.
