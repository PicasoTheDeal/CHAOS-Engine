# dev registry key

`server.key` is a throwaway ed25519 key for `chaos-registry-dev` and the vectors tool, so they can sign tickets and
certs without a server. it is not in the repo: make one with

```
client/build/chaos-registry-dev keygen --key server/dev/server.key
```

(leave `--pub` off, or you overwrite `shared/server_pubkey.h`, which holds the production public key.) a client built
with the production key ignores anything this key signs, so it proves nothing to anyone.
