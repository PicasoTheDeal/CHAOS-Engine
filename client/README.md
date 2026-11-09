# CHAOS engine, client

c++20 / opengl 4.3 core / glfw 3.4 (3.3 builds too, x11 only there). `../Design map` for the why, `../Data and logic flow` for the how.

## build

debian / ubuntu / kali:

```
sudo apt install build-essential cmake ninja-build pkg-config libglfw3-dev libglm-dev libsodium-dev libssl-dev \
  libcurl4-openssl-dev catch2 nlohmann-json3-dev libwayland-dev libegl-dev libx11-dev libxext-dev libxrandr-dev libgl-dev
```

arch: `glfw glm libsodium openssl curl catch2 nlohmann-json cmake ninja libx11 libxext wayland libglvnd`. fedora names aer in the root readme.

```
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build client/build
./client/build/chaos_tests
./client/build/chaos                          # random seed
./client/build/chaos --seed <64 hex | base58 | 24 words>   # replay a seed
./client/build/chaos --seed <hex> --shot out.png   # 1920x1080 offscreen frame, no window
./client/build/chaos --shape 27 --mood 0.9        # force a generator (see --list-shapes) and pin the mood
./client/build/chaos --mode bleed                 # show a mode other than the seed's native one
./client/build/chaos --seed <x> --shot p.png --keep   # also write the .chaos + png into the vault
./client/build/chaos --vault                      # list the vault, each file verified against the registry key
./client/build/chaos --verify f.chaos             # check one file
./client/build/chaos --import f.chaos             # verify + store (a claimed file coming back from the registry)
./client/build/chaos --commit out.reveal                       # step 1 of a claimable roll by hand: prints the commit
./client/build/chaos --ticket t.ticket --reveal out.reveal --shot p.png --keep   # step 2: the seed derives from ticket + reveal
```

seeds: all 32 bytes drive the picture, hex, base58 and the 24 bip39 words aer the same seed. keys: Esc panel, R preview roll, K keep, M mood, N mode, 1-4 set a mode, Space pause, F fullscreen, V desk / full view, Ctrl+Q quit.

the window opens on the desk: the chaos is a preview on the left with the big keys under it (roll, claimable roll, keep, claim, set as wallpaper, full view), the tabs on the right. V (or the full view key) makes the chaos fill the window with the panel floating over it, like before. keep always writes a 1920x1080 frame whatever the preview size is.

vendored in `third_party/`: glad2 (generated, gl core 4.3), blake3 portable c, stb, dear imgui (only the files the panel uses, see the VENDORED note in there).

## layout

```
src/seed       splitmix64, xoshiro256**, blake3 sub streams, base58, bip39, rarity + families
src/shape      three.js geometry equivalents, 41 generators, mutation lineage
src/scene      dimension roll (everything one seed decides), palette, mood, modes, props the registry derives
src/render     shader loader (#include <file> supported), gl mesh, post stack, the renderer
src/model      .cheng: import (obj / stl / gltf / glb / blender), clean, pack, read
src/chaos_file .chaos read / write / derive / verify / sign
src/vault      local vault, activation list, instance id
src/sync       registry api client (libcurl), roll / claim / sync flows
src/ui         imgui panel, boot poster, the mark, thumbnails
src/platform   window (glfw), x11 wallpaper hints, wayland layer shell, battery
src/wallpaper  the --wallpaper loop, src/wallpaper_seed the seed rule, src/settings settings.json
src/cli        every flag that isn't the window
plasma/        the kde plasma 6 wallpaper plugin (qml module + package)
tools/         chaos-registry-dev (the registry's half in c++, for local play), chaos-vectors
shaders/       glsl 430 ports of the three.js shaders + the new ones (own, blit, bloom_duotone)
tests/         catch2
```

## if startup hangs on wayland

glfw loads libdecor for client side decorations and libdecor's gtk plugin can block forever in a container or a stripped session. yes forever. `chaos --no-libdecor` skips it (kde decorates server side anyway), or `LIBDECOR_PLUGIN_DIR=/nonexistent chaos`. `chaos --x11` runs the normal window through xwayland instead.

## packaging

```
cmake --install client/build --prefix /usr/local        # bin/chaos, share/chaos/{shaders,assets}, .desktop, icon
(cd client/build && cpack -G "DEB;RPM;TGZ")             # chaos-engine_1.1.0_amd64.deb, chaos-engine-1.1.0-1.x86_64.rpm, .tar.gz (rpm needs rpmbuild)
packaging/build-appimage.sh                             # dist/CHAOS-x86_64.AppImage (downloads linuxdeploy + appimagetool once)
packaging/build-portable.sh                             # all of the above inside debian bookworm (podman / docker): runs on old glibc, no plasma plugin
packaging/build-arch.sh                                 # the two arch packages from this tree (chaos-engine, chaos-engine-plasma), no git needed
makepkg -si -p packaging/PKGBUILD                       # arch, from the git tree
chaos --version
```

the binary finds its shaders and assets next to itself, under `../share/chaos`, or at the build time path, so the same build runs from the tree, a prefix, the .deb or the appimage. packages built on this machine link against this machine's glibc, so for other distros build them in the bookworm container or they just won't install, learned that one the hard way.

## registry session

```
./client/build/chaos --login-token <token>     # from <registry>/login?app=1 (or --login-dev NAME on a dev registry)
./client/build/chaos --whoami
./client/build/chaos --roll --shot p.png --keep  # claimable roll in one go: commit, ticket, seed
./client/build/chaos --claim ~/.local/share/chaos/vault/<short>.chaos
./client/build/chaos --sync                      # states for what you hold, fetch what you own but don't hold
./client/build/chaos --logout
```

the panel's account tab does the same: open the sign in page, paste the token, connect, sync. the setup tab has the whole thing as 4 big keys.

## online rule

the app pings the registry at start and every 30 s (`--registry URL`, `CHAOS_REGISTRY`, or `registry_url` in settings.json, default https://chaos-eng.tetstack.dev). nothing generates without an account, previews included. online and signed in: everything, and loading a vault file asks the registry and records the answer in `activated.json`. offline: only what's in your vault or activated plays, no previews, no rerolls, no arbitrary `--seed`. `--dev-offline` lifts the gate for development. needs libcurl.

## kde plasma, as a real wallpaper option

on kde the layer shell window lands in the same layer as plasma's own desktop and covers it, icons and widgets included. so there's a plasma wallpaper plugin that renders the engine inside plasmashell and shows up in desktop settings as wallpaper type CHAOS.

```
sudo pacman -S --needed qt6-base qt6-declarative plasma-workspace extra-cmake-modules
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCHAOS_PLASMA=ON
cmake --build client/build
sudo cmake --install client/build        # chaos, shaders + assets to /usr/share/chaos, the qml module, the wallpaper package
kquitapp6 plasmashell; kstart plasmashell   # or log out and in
```

or from the app: "set as desktop wallpaper" (setup or view tab) switches every desktop to the plugin with the current seed and your model through plasmashell's scripting interface.

then right click the desktop, configure desktop and wallpaper, wallpaper type CHAOS. the settings page has a vault picker, seed, mode, draw (everything / no satellites / main shape alone), fps (50 and up locks to the display refresh, 30 is the light one), render size (3/4 by default, the easy win on 4k), anti aliasing, lite, battery pause, pause under a fullscreen window and slow under a maximised one (off by default), your model and its skin, the registry, and a live status line. everything it says also lands in the log, so if the desktop looks wrong:

```
tail -n 20 ~/.cache/chaos/plasma.log
```

if the plugin doesn't appear at all or the desktop stays plain black with nothing in the log, plasmashell could not load the module, `journalctl --user -u plasma-plasmashell.service -b | grep -i chaos` says why. the plugin links with `--no-undefined` so a missing symbol fails the build instead of a silent black desktop (been there).

## where the files are

every kept roll, claim and sync lands in your vault folder as `<seed>.chaos` + `.png`. default `~/.local/share/chaos/vault`, the account tab lets you pick any folder (`vault_dir` in settings.json) and the plugin reads the same one. converted models go in `models/` next to it.

## your own model on the stage (.cheng)

the model tab (or `chaos --cheng file`) turns an `.obj`, `.stl`, `.gltf` or `.glb` (and `.blend`, `.fbx`, `.dae`, `.step`, `.usd` through blender when it's installed) into a `.cheng`: welded with its seams kept, thinned only past 120k vertices, sized for the stage. materials come along: base colours, vertex colours, roughness / metal / glow and one base colour texture (the one covering most of the model, halved till it fits 1024, stored as a png inside the file). a model with several materials keeps them as vertex colours. on the stage it replaces the main shape and the satellites go. skin own (what it brought), berry (a seed's skin, pick which seed with "look from"), mono, wire, or random. the duotone leaves a model wearing its own look alone so your textures keep their colours while the backdrop stays the seed's. a display choice, offline, never a seed. `chaos --model file` shows one, the plasma wallpaper carries it when you press "set as desktop wallpaper".

## measuring

`chaos --bench 120` renders 120 frames offscreen at 1080p with the saved quality settings and prints the time per frame and the memory used. `CHAOS_PROFILE=1 chaos --bench 120` adds the gpu time of every pass.

## display settings (the app, the wallpaper and the plugin)

the prefs tab has a display section: draw (everything / no satellites / main shape alone), render size (full / 3/4 / half), anti aliasing (off / 2x / 4x) and detail (full / lite). this install's choice, changes nothing about a seed, its value or its cert. if it lags press "low-end preset" (60% size, no aa, lite, no satellites, 30 fps). the keys live in settings.json (`layers`, `render_scale`, `msaa`, `detail`) and wallpaper mode reads the same ones, the plugin has them on its page.

## wallpaper backends

`chaos --wallpaper` picks the wayland layer shell when the compositor has it (sway, hyprland, river, kwin), otherwise the x11 desktop window (xwayland under a wayland session). `--backend wayland|x11` forces one. `--monitors mirror|each`, `--wallpaper-fps N`, `--frames N` (testing), `--no-pause`. no fullscreen app pause on the wayland path (compositors don't tell you), battery pause on both, both off by default.

## the by-hand roll, without a session

`chaos --commit out.reveal` prints a commit. paste it on the site's vault page, it gives you a `chaos.ticket` (that's your one roll for the week). then from the folder both files are in: `chaos --ticket chaos.ticket --reveal out.reveal --shot p.png --keep`. a ticket the registry signed is the session for that roll, no login needed. don't run `--commit` again in between, the ticket only matches the reveal it was made for (and `--commit` refuses to overwrite an existing out.reveal for exactly that reason).

## the .chaos flow by hand (the dev tool)

`chaos-registry-dev` is the registry's half in c++ (keygen, ticket, claim, transfer, verify) with a flat text db, so the whole claim and transfer loop runs locally:

```
client/build/chaos-registry-dev keygen --key server/dev/server.key   # once, leave --pub off
client/build/chaos --commit out.reveal                 # prints COMMIT
client/build/chaos-registry-dev ticket --db r.db --user 7 --commit COMMIT --out t.ticket
client/build/chaos --ticket t.ticket --reveal out.reveal --shot p.png --keep
client/build/chaos-registry-dev claim --db r.db --in ~/.local/share/chaos/vault/<short>.chaos --out claimed.chaos
client/build/chaos --import claimed.chaos
client/build/chaos-registry-dev transfer --db r.db --in claimed.chaos --to 9 --out theirs.chaos
```

the dev key is not the production key and nothing built with the production key trusts it.

## history, short

v0.1 window + one shape. v0.2 the full scene. v0.3 modes. v0.4 seed forms, rarity, vault. v0.5 the .chaos file and the dev registry tool. v0.6 the imgui panel + poster. v0.7 x11 wallpaper. v0.8 / v0.9 the registry. v1.0 sessions + sync. v1.1 packaging. v1.2 wayland layer shell + the plasma plugin. v1.3 market polish, previews, config. v1.4 the kde pass (the look, perf, setup tab, .cheng, docs on the site). v1.5 packages per distro for the testers. the long version is in the design map, section 16.
