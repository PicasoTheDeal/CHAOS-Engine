# CHAOS

So this is CHAOS. A 3D dimension rolled from a seed, running as your Linux wallpaper, and the registry that says which ones are yours.

I had a web toy before (chaos.tetstack.dev, three.js) and this is the real thing: a C++ engine, a KDE wallpaper plugin, and a website on Cloudflare that keeps the books (the registry, which lives in its own repo). Same idea as before, 256 bits in and a whole dimension out, but now a seed can be owned, valued and traded, and nobody can cheat the rare ones.

## What's in here

**The engine.** C++20 and OpenGL 4.3. 41 shape generators (40 real ones plus a compound that fuses 2 to 4 of them), a mutation lineage on top, the berry skin shader, the hyperspace backdrop, satellites, debris, rings, particles, and a post stack (bloom, duotone, film, afterimage). Every part pulls from its own stream of randomness hashed off the seed, so the same seed gives the same dimension on any pc, forever. 4 modes (zen, calm, chaos, bleed), a mood slider, palette jumps, a mutation clock. It runs as a window with a panel, as a window-less wallpaper on Wayland (layer shell) and X11, and as a Plasma wallpaper plugin for KDE, which is the one I actually use.

**Rarity.** Every seed rolls a tier with fixed odds (60 / 25 / 10 / 4 / 0.9 / 0.1) and the tier unlocks stuff you can see: more generators in the lineage, berry satellites, a twin, a third palette stop, forced compound shapes, gold. The odds live in code on both sides on purpose, they're protocol not config.

**The .chaos file.** 512 bytes. The seed, the ticket, the cert. It's a receipt basically, the registry row is what counts. Check any .chaos on the site and it tells you if it's real, what it is, and what the registry says about that exact copy.

**The roll.** Commit-reveal so nobody can grind. The app commits to its random first, the registry adds its own and signs a ticket, the seed is the hash of both. The registry derives the tier itself on claim and ignores whatever the client wrote. One claimable roll a week per account, and you need an account to roll at all. Previews are free and worth nothing.

**The registry.** Not in this repo, it's private (chaos-eng.tetstack.dev is the live one). One SvelteKit app on Cloudflare with D1 as the only storage. Google sign in only, no passwords anywhere. Check, claim, vault, market, a public page per seed, and a docs section with the actual code chunks for each part plus the design map and the flow doc as they are. Every input goes through one validator, there's a honeypot for scrapers, CSP and the usual headers, and a rate limit in front of it all. Money isn't wired yet, acquire moves ownership at the listed value on paper. That's v2.0.

**Your own models (.cheng).** Drop an obj / stl / gltf / glb (or blend, fbx, dae, step, usd through blender if you have it) and the app cleans it, keeps every seam and groove (it only thins a mesh past 120k vertices), packs it small and puts it on the stage instead of the seed's shape. It brings its materials too: base colours, vertex colours, roughness, metal, glow and one base-colour texture packed inside the .cheng, and the duotone leaves those pixels alone so your texture keeps its colours. Or skin it mono, wire, or wearing the skin of any seed you pick from your vault. Offline thing, never a seed, never claimable.

**The look.** Flat, drafted, lines that run past the corners, slate and bone with an amber accent, a paper mode too. The mark is a 16x16 pixel thing, order on the left falling apart on the right. Fonts come from your desktop. No rounded corners, no gradients, nothing that looks like it came out of a template.

**Low-end stuff.** Render size, anti-aliasing, lite detail, draw less (no satellites, main shape alone), a low-end preset, pause on battery, pause when a fullscreen window covers the desktop. `chaos --bench 120` tells you what each knob buys on your machine.

## Running it

Engine:

```
cmake -S client -B client/build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCHAOS_PLASMA=ON
cmake --build client/build && sudo cmake --install client/build
chaos
```

The registry (the site) is a separate private repo, so there's nothing to run for it here. The app talks to the live one, and a client built with the production public key (`shared/server_pubkey.h`) only trusts that one.

The why of everything is in `Design map`, the how in `Data and logic flow`, and both are on the site under docs too. `client/README.md` has the deps per distro and the details.

## Where things stand

Everything through the registry, the plugin, the model pipeline and the docs is done and running. Next is v2.0: payments on the market (store credit first, then payouts), maybe global caps per tier so rarity holds no matter how many accounts there are, and binding an install to an account so multi-accounting doesn't leak rolls. The blockchain mirror idea is parked unless buyers ask for it.
