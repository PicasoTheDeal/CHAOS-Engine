# CHAOS

so this is CHAOS. a 3D dimension rolled from a seed, running as your linux wallpaper, and a registry that says which ones aer yours.

I had a web toy before (chaos.tetstack.dev, three.js) and this is the real thing: a c++ engine, a kde wallpaper plugin, and a site on cloudflare that keeps the books (the registry, its own private repo). same idea as before, 256 bits in and a whole dimension out, but now a seed can be owned, valued and traded, and nobody can cheat the rare ones.

## what's in here

**the engine.** c++20 and opengl 4.3. 41 shape generators (40 real ones + a compound that fuses 2 to 4 of them), a mutation lineage on top, the berry skin shader, the hyperspace backdrop, satellites, debris, rings, particles, and the post stack (bloom, duotone, film, afterimage). every part pulls from its own stream of randomness hashed off the seed, so the same seed gives the same dimension on any pc, forever. 4 modes (zen, calm, chaos, bleed), a mood slider, palette jumps, a mutation clock. it runs as a window with a panel, as a window-less wallpaper on wayland (layer shell) and x11, and as a plasma wallpaper plugin for kde, which is the one I actually use.

**rarity.** every seed rolls a tier with fixed odds (60 / 25 / 10 / 4 / 0.9 / 0.1) and the tier unlocks stuff you can see: more generators in the lineage, berry satellites, a twin, a third palette stop, forced compound shapes, gold. the odds live in code on both sides on purpose, they're protocol not config.

**the .chaos file.** 512 bytes. the seed, the ticket, the cert. it's a receipt basically, the registry row is what counts. check any .chaos on the site and it tells you if it's real, what it is, and what the registry says about that exact copy.

**the roll.** commit reveal so nobody can grind. the app commits to its random first, the registry adds its own and signs a ticket, the seed is the hash of both. the registry derives the tier itself on claim and ignores whatever the client wrote. one claimable roll a week per account, and you need an account to roll at all. previews aer free and worth nothing.

**the registry.** not in this repo, it's private (chaos-eng.tetstack.dev is the live one). one sveltekit app on cloudflare with D1 as the only storage. google sign in only, no passwords anywhere. check, claim, vault, market, a public page per seed, and a docs section with the actual code chunks for each part plus this design map and the flow doc as they are. every input goes through one validator, there's a honeypot for scrapers, csp and the usual headers, and a rate limit in front of it all. money isn't wired yet, acquire moves ownership at the listed value on paper. that's v2.0.

**your own models (.cheng).** drop an obj / stl / gltf / glb (or blend, fbx, dae, step, usd through blender if you have it) and the app cleans it, keeps every seam and groove (it only thins a mesh past 120k vertices), packs it small and puts it on the stage instead of the seed's shape. it brings its materials too: base colours, vertex colours, roughness, metal, glow and one base colour texture packed inside the .cheng, and the duotone leaves those pixels alone so your texture keeps its colours. or skin it mono, wire, or wearing the skin of any seed from your vault. offline thing, never a seed, never claimable.

**the look.** flat, drafted, lines that run past the corners, slate and bone with an amber accent, a paper mode too. the mark is a 16x16 pixel thing, order on the left falling apart on the right. fonts come from your desktop. no rounded corners, no gradients, nothing that looks like it came out of a template.

**low end stuff.** render size, anti aliasing, lite detail, draw less (no satellites, main shape alone), a low end preset, pause on battery, pause when a fullscreen window covers the desktop. `chaos --bench 120` tells you what each knob buys on your machine.

## installing it

grab the file for your distro from the releases page (or whatever I sent you on telegram) and:

**kali / debian / ubuntu**

```
sudo apt install ./chaos-engine_1.1.0_amd64.deb
```

**garuda / arch / manjaro** (this one has the kde plasma wallpaper plugin in it)

```
sudo pacman -U chaos-engine-1.1.0-1-x86_64.pkg.tar.zst
```

**fedora / nobara**

```
sudo dnf install ./chaos-engine-1.1.0-1.x86_64.rpm
```

**anything else, or if the package above argues with your libs**

```
chmod +x CHAOS-x86_64.AppImage && ./CHAOS-x86_64.AppImage
```

then `chaos` opens the app. the setup tab walks you through it: sign in, roll, keep, set as wallpaper.

**as your actual wallpaper**

- kde plasma 6 on arch: right click the desktop, configure desktop and wallpaper, wallpaper type CHAOS. on the other distros the plugin needs a build against your plasma, so use the next line instead.
- hyprland, sway, kde wayland, anything with layer shell: `chaos --wallpaper`. it plays the seed you kept last (or the lobby when you have none). autostart on hyprland is one line in `~/.config/hypr/hyprland.conf`: `exec-once = chaos --wallpaper`. sway: `exec chaos --wallpaper`.
- x11: same command, it goes behind your icons as a desktop window.
- windows with wsl: the app runs under wslg, the wallpaper can't, there's no linux desktop to be the wallpaper of. `chaos --seed <hex> --shot out.png` and set the png yourself.

it needs opengl 4.3. intel from haswell up, any amd on mesa, nvidia on the proprietary driver all do it. if it opens black or dies, `chaos --version` and `~/.cache/chaos/plasma.log` (plugin) aer what I need from you.

## building it yourself

no package for you yet, or you want the latest? deps first:

kali / debian / ubuntu:

```
sudo apt install build-essential cmake ninja-build pkg-config git libglfw3-dev libglm-dev libsodium-dev libssl-dev libcurl4-openssl-dev catch2 nlohmann-json3-dev libwayland-dev libegl-dev libx11-dev libxext-dev libxrandr-dev libgl-dev
```

fedora:

```
sudo dnf install gcc-c++ cmake ninja-build pkgconf git glfw-devel glm-devel libsodium-devel openssl-devel libcurl-devel catch2-devel json-devel wayland-devel mesa-libEGL-devel libX11-devel libXext-devel mesa-libGL-devel
```

garuda / arch (add `qt6-declarative plasma-workspace extra-cmake-modules` for the plasma plugin):

```
sudo pacman -S --needed base-devel cmake ninja git catch2 glm nlohmann-json glfw libsodium openssl curl libx11 libxext wayland libglvnd
```

then, from the repo:

```
cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build client/build && sudo cmake --install client/build
chaos
```

add `-DCHAOS_PLASMA=ON` to the first line if you're on kde plasma 6 and installed its dev packages. ubuntu 22.04 and debian 12 ship an old catch2 so the tests just get skipped there, the app still builds.

## making the packages

`packaging/build-portable.sh` builds the .deb, .rpm, .tar.gz and the appimage inside a debian bookworm container (podman or docker), cuz a .deb built on arch wants arch's glibc and won't install anywhere else. `packaging/build-arch.sh` builds the arch package on an arch machine, plasma plugin included. both drop into `dist/`. the ones I ship go on the releases page, nothing built is in git.

the registry (the site) is a seperate private repo, so there's nothing to run for it here. the app talks to the live one, and a client built with the production public key (`shared/server_pubkey.h`) only trusts that one.

the why of everything is in `Design map`, the how in `Data and logic flow`, both aer on the site under docs too. `client/README.md` has the details.

## licence, sort of

the repo is public so people can read it and test it. it's not open source: don't ship it, fork it into smthing else or sell it. if you want to build on it, ask me first.

## where things stand

everything through the registry, the plugin, the model pipeline, the docs and the packages is done and running. next is v2.0: payments on the market (store credit first, then payouts), maybe global caps per tier so rarity holds no matter how many accounts there are, and binding an install to an account so multi accounting doesn't leak rolls. the blockchain mirror idea is parked unless buyers ask for it.
