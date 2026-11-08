# ownership model

the short version of section 11 in the design map.

- the app renders any seed offline. owning one needs a google login on the registry site.
- roll online: the app commits to its random, asks for a ticket, the registry enforces the allowance (1 a week), the seed comes out of the ticket + both randoms, the app writes a `.chaos` (state rolled).
- roll offline: no ticket. state preview. never claimable. (and since 2026-11-06 the app doesn't even do previews without an account, so "offline" means replaying what you already have)
- claim: upload the file. the registry verifies the ticket, recomputes the seed, derives rarity itself, computes value, signs the cert, gives the signed file back. first valid upload of a seed wins.
- transfer / gift: owner lists on the site, buyer acquires, the registry rotates the nonce and re-signs. old copies aer dead.
- copying a file does nothing. only the registry moves ownership.
- value is a market thing set by the registry (floor per tier times multipliers). rarity is a seed thing and never changes.

`docs/chaos-file-format.md` has the bytes.
