# dev registry key

`server.key` is a throwaway ed25519 private key (libsodium 64-byte secret key format) whose public half is in
`shared/server_pubkey.h`. It exists so `chaos-registry-dev` and the tests can sign tickets and certs without a
server. It is committed on purpose. It is NOT the production key. When the real registry ships: generate a new
pair on the server, paste the public key into `shared/server_pubkey.h`, keep the private key off the repo.
