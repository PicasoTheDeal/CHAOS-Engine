# CHAOS engine, client

C++20 / OpenGL 4.3 core / GLFW 3.4. See `../Design map` for why.

## Build

Debian/Ubuntu packages:

```
sudo apt install build-essential cmake ninja-build pkg-config libglfw3-dev libglm-dev libsodium-dev libssl-dev \
  libfreetype-dev libsecret-1-dev libcurl4-openssl-dev libargon2-dev catch2 nlohmann-json3-dev libspdlog-dev libwayland-dev libegl-dev
```

Arch: `glfw glm libsodium openssl freetype2 libsecret curl argon2 catch2 nlohmann-json spdlog cmake ninja libx11 libxext wayland libglvnd`.

```
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build client/build
./client/build/chaos_tests
./client/build/chaos                          # random seed
./client/build/chaos --seed <64 hex | base58 | 24 words>   # replay a seed
./client/build/chaos --seed <hex> --shot out.png   # 1920x1080 offscreen preview, no window
./client/build/chaos --shape 27 --mood 0.9        # force a generator (see --list-shapes) and pin the mood
./client/build/chaos --mode bleed                 # show a mode other than the seed's native one
./client/build/chaos --seed <x> --shot p.png --keep   # also write the .chaos + png into the vault
./client/build/chaos --vault                      # list the vault, each file verified against the registry key
./client/build/chaos --verify f.chaos             # check one file
./client/build/chaos --import f.chaos             # verify + store (a claimed file coming back from the registry)
./client/build/chaos --commit out.reveal                       # step 1 of a claimable roll: prints the commit for the ticket request
./client/build/chaos --ticket t.ticket --reveal out.reveal --shot p.png --keep   # step 2: the seed derives from ticket + reveal
```

Seeds: all 32 bytes drive the picture; hex, base58 and 24 bip39 words are the same seed. Keys: M cycles mood, N cycles mode, 1-4 set a mode, K keeps the frame to the vault, Esc quits.

Vendored in `third_party/`: glad2 (generated, gl core 4.3), BLAKE3 portable C, stb, Dear ImGui.

## Layout

```
src/platform   window + context (glfw)
src/render     shader loader (#include <file> supported), VAO/VBO mesh
src/shape      three.js geometry equivalents, 41 generators, mutation lineage
src/scene      dimension roll (everything one seed decides), palette, mood
src/render     also post stack (bloom, duotone, film, afterimage) and the renderer
src/seed       splitmix64, xoshiro256**, blake3 sub-streams, base58, bip39, rarity + families
src/vault      local vault, instance id
src/chaos_file .chaos read/write/derive/verify/sign
src/ui         imgui panel, boot poster, thumbnails
src/platform   window, x11 wallpaper hints, pause probes
src/wallpaper  the --wallpaper loop; src/settings settings.json + autostart
tools/         chaos-registry-dev
src/util       png screenshot
shaders/       GLSL 430 ports of the reference three.js shaders
tests/         catch2
```

## If startup hangs on Wayland

GLFW loads libdecor for client-side decorations, and libdecor's GTK plugin can block forever in a container or a
stripped session. `chaos --no-libdecor` skips it (KDE decorates server side anyway), or `LIBDECOR_PLUGIN_DIR=/nonexistent chaos`.
`chaos --x11` runs the normal window through XWayland instead.

## Packaging (v1.1)

```
cmake --install client/build --prefix /usr/local        # bin/chaos, share/chaos/{shaders,assets}, .desktop, icon
(cd client/build && cpack -G "DEB;TGZ")                 # chaos-engine_1.1.0_amd64.deb, chaos-engine-1.1.0-Linux.tar.gz
packaging/build-appimage.sh                             # dist/CHAOS-x86_64.AppImage (downloads linuxdeploy + appimagetool once)
makepkg -si -p packaging/PKGBUILD                       # arch, from the git tree
chaos --version
```

The binary finds its shaders and assets next to itself, under `../share/chaos`, or at the build-time path, so the
same build runs from the tree, a prefix, the .deb or the AppImage. Deploying the registry: `docs/deploy.md`.

## Registry session (v1.0)

```
./client/build/chaos --login-token <token>     # from <registry>/login?app=1 (or --login-dev NAME on a dev registry)
./client/build/chaos --whoami
./client/build/chaos --roll --shot p.png --keep  # claimable roll in one go: commit, ticket, seed
./client/build/chaos --claim ~/.local/share/chaos/vault/<short>.chaos
./client/build/chaos --sync                      # states for what you hold, fetch what you own but do not hold
./client/build/chaos --logout
```

The panel's account tab does the same: open the sign-in page, paste the token, connect, sync. The roll tab gets a
one-button claimable roll and a claim button once connected.

## Online rule

The app pings the registry at start (`--registry URL`, `CHAOS_REGISTRY`, or `registry_url` in settings.json; default
https://chaos-eng.tetstack.dev). Online: everything, and loading a vault file (`--import`, or load in the panel)
asks the registry and records the answer in `~/.local/share/chaos/activated.json`. Offline: nothing is generated
(no previews, rerolls, tickets, or arbitrary `--seed`); only activated files load, and wallpaper mode plays an
activated seed. `--dev-offline` lifts the gate for development. Needs libcurl.

## KDE Plasma: as a real wallpaper option

On KDE the layer-shell window lands in the same layer as Plasma's own desktop and covers it (icons and widgets
included). The Plasma wallpaper plugin renders the engine inside plasmashell instead, and shows up in
Desktop settings as a wallpaper type called CHAOS.

```
sudo pacman -S --needed qt6-base qt6-declarative kirigami
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCHAOS_PLASMA=ON
cmake --build client/build
sudo cmake --install client/build        # chaos, shaders + assets to /usr/share/chaos, the qml module, the wallpaper package
kquitapp6 plasmashell; kstart plasmashell   # or log out and in
```

Or, from the app: view tab (or the setup tab), "set as desktop wallpaper" switches every desktop to the plugin with
the current seed through plasmashell's scripting interface (needs qdbus6, qdbus or gdbus).

Then right-click the desktop, Configure Desktop and Wallpaper, Wallpaper type: CHAOS. The settings page has a vault
picker, seed, mode, what to draw (everything / no satellites / the main shape alone), fps (50 and above locks to the
display refresh, 30 is the light option), render size (full / 3/4 / half, the easy win on 4K), registry and battery
pause, "pause under a fullscreen window (games), slow under a maximised one", your model, plus a live status line. Draw and size are this screen's choice only; the chaos, its value and its
certificate do not change. It shares the app's vault and
activation list, so the same online rule applies: empty seed = random while the registry is reachable, otherwise an
activated or kept one (offline, anything in your vault plays; only generating needs the registry). When nothing may
play (offline with an empty vault, registry unset, bad seed) it draws the lobby,
one fixed public seed, instead of a black desktop. Everything the wallpaper reports also lands in
`~/.cache/chaos/plasma.log`, so if the desktop looks wrong:

```
tail -n 20 ~/.cache/chaos/plasma.log
```

If the plugin does not appear at all or the desktop stays plain black with nothing in the log, plasmashell could not
load the module; `journalctl --user -u plasma-plasmashell.service -b | grep -i chaos` says why. The plugin links
with `--no-undefined`, so a missing symbol fails the build instead.

## Where the files are

Every kept roll, claim and sync lands in the vault folder as `<seed>.chaos` plus a `.png`. The default is
`~/.local/share/chaos/vault`; the account tab lets you choose any folder (saved as `vault_dir` in settings.json) and
the wallpaper plugin reads the same one.

## Rolling needs an account

Nothing is generated without a session, previews included. Sign in at `<registry>/login?app=1`, paste the token on
the account tab (or `chaos --login-token <token>`), then roll. Claimable rolls are one per week per account. Files
already in your vault play without a session; the lobby plays when nothing else may.

## Your own model on the stage (.cheng)

The model tab (or `chaos --cheng file`) turns an `.obj`, `.stl`, `.gltf` or `.glb` (and `.blend`, `.fbx`, `.dae`,
`.step`, `.usd` through blender when it is installed) into a `.cheng`: welded with its seams kept, thinned only past 120k vertices, sized for the stage. Materials come along: base colours, vertex colours, roughness / metal / glow and one base-colour texture (the one covering most of the model, halved until it fits 1024, stored as a png inside the file). A model with several materials keeps them as vertex colours. On the stage it replaces the main shape and the satellites go; skin own (what it brought), berry / mono / wire or
random, one colour of your choice. The duotone leaves a model wearing its own look alone, so your textures keep their colours while the backdrop stays the seed's. A display choice, offline, never a seed. `chaos --model file` shows one; the
Plasma wallpaper carries it when you press "set as desktop wallpaper".

## Measuring

`chaos --bench 120` renders 120 frames offscreen at 1080p with the saved quality settings and prints the time per
frame and the memory used. `CHAOS_PROFILE=1 chaos --bench 120` adds the GPU time of every pass.

## Display settings (the app, the wallpaper and the plugin)

The settings tab has a display section: draw (everything / no satellites / the main shape alone), render size
(full / 3/4 / half) and anti-aliasing (off / 2x / 4x) and detail (full / lite). They are this install's choice and change nothing about a
seed, its value or its certificate. If the app lags, press "low-end preset" (60% size, no anti-aliasing, lite detail, no satellites, 30 fps). The keys live in
`~/.config/chaos/settings.json` (`layers`, `render_scale`, `msaa`) and wallpaper mode reads the same ones; the
Plasma plugin has the same three on its settings page.

## Wallpaper backends (v1.2)

`chaos --wallpaper` picks the native Wayland layer shell when the compositor offers it (KDE, sway, Hyprland, river),
otherwise the X11 desktop-window path (XWayland under a Wayland session). `--backend wayland|x11` forces one.
No fullscreen-app pause on the Wayland path; battery pause works on both.

## v0.7 status

Wallpaper mode: `chaos --wallpaper` forces X11 (XWayland under a Wayland session) and puts one desktop-layer window
per monitor (`_NET_WM_WINDOW_TYPE_DESKTOP`, below, sticky, no input). `--monitors mirror|each`, `--wallpaper-fps N`,
`--frames N` (testing), `--no-pause`. Pauses when the active window is fullscreen or the machine is on battery.
Reads `~/.config/chaos/settings.json` (edited from the panel's settings tab, which also toggles XDG autostart).
Arch needs `libx11 libxext nlohmann-json` on top of the earlier list.

## v0.6 status

Dear ImGui control panel (Esc): vault with thumbnails and verification, roll (preview / claimable from a ticket /
any seed form), view (seed forms with copy, lineage, mood slider, mode, palette), settings (vsync, fps cap, pause,
monitor, fullscreen, poster), account (placeholders for v1.0). Boot poster with the real build stages and the web
fonts (converted to ttf in `assets/fonts`). Keys: Esc panel, R reroll, K keep, M mood, N mode, 1-4 mode, Space pause,
F fullscreen, Ctrl+Q quit. `--no-panel`, `--no-poster`, `--ui-shot out.png` (hidden-window screenshot with the UI).

## v0.5 status

The .chaos flow: read, write, seed derivation from a registry ticket, offline verification of ticket and cert
signatures (ed25519, libsodium) against the embedded registry public key. `chaos-registry-dev` is the server's half
(keygen, ticket, claim, transfer, verify) with a flat text db, so the whole claim and transfer loop can be run locally:

```
client/build/chaos --commit out.reveal                 # prints COMMIT
client/build/chaos-registry-dev ticket --db r.db --user 7 --commit COMMIT --out t.ticket
client/build/chaos --ticket t.ticket --reveal out.reveal --shot p.png --keep
client/build/chaos-registry-dev claim --db r.db --in ~/.local/share/chaos/vault/<short>.chaos --out claimed.chaos
client/build/chaos --import claimed.chaos
client/build/chaos-registry-dev transfer --db r.db --in claimed.chaos --to 9 --out theirs.chaos
```

The dev key in `server/dev/server.key` is committed for tests and local play. It is not the production key.

## v0.4 status

Seed forms (base58, 24-word bip39), rarity tiers rolled on their own stream with visual unlocks (extra lineage, berry
satellites, a second berry, a third duotone stop, forced compound, bleed, doubled rings, gold), the 16 family buckets,
the local vault (`~/.local/share/chaos/vault`, preview-state .chaos + png) and a per-install instance id.
Needs libcrypto (OpenSSL) for the bip39 checksum and libsodium for ed25519.

## v0.3 status

Modes: every seed rolls a native mode (zen / calm / chaos / bleed, 20/35/35/10). The display mode can be overridden
locally; the native one is what the registry values. Zen pins the mood to 0, stops palette jumps and the mutation
clock, and hides the swarm. Calm caps the mood and never hard-cuts. Bleed pins wild, hard-cuts 60% of the time,
holds the afterimage at 0.96 and adds a chromatic smear pass.

## v0.2 status

The full reference scene: 41 shape generators, mutation lineage, berry skin, hyperspace backdrop, wire ghost,
satellites (berry or glitch), fold planes, debris, particles, rings, mood engine, palette jumps, mutation clock,
and the post stack (bloom, duotone, film, afterimage) with 4x MSAA. Not ported: shadow maps, orbit controls.

