# Ownership model

- The app renders any seed offline. Owning one requires a Google login on the registry website.
- Roll online: app requests a generation ticket, server enforces allowance, app derives the seed from the ticket
  plus local entropy, writes a `.chaos` file (state rolled).
- Roll offline: same, minus the ticket. State preview. Never claimable.
- Claim: upload the file. Server verifies the ticket, recomputes the seed, derives rarity, computes value, signs the
  cert, returns the signed file. First valid upload of a seed wins.
- Transfer / gift: owner lists on the site, buyer acquires, server rotates nonce and re-signs. Old copies are dead.
- Copying a file does nothing. Only the registry moves ownership.
- Value is a market property set by the registry (floor per tier times multipliers). Rarity is a seed property and
  never changes.

See `docs/chaos-file-format.md` for the bytes and `Design map` section 4 for the full flow.
