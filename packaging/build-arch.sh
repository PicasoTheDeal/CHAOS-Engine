#!/usr/bin/env bash
# builds the arch / garuda / manjaro packages from this tree. no git clone, no github: the tree is tarred into dist/
# and makepkg builds that. run it on an arch machine.
# out: dist/chaos-engine-<ver>-1-x86_64.pkg.tar.zst          the app + wallpaper mode, light deps, no kde needed
#      dist/chaos-engine-plasma-<ver>-1-x86_64.pkg.tar.zst   the kde plasma 6 wallpaper plugin, pulls plasma-workspace (only for kde people)
# install: sudo pacman -U dist/chaos-engine-*.pkg.tar.zst          and on kde also the -plasma one
# usage: packaging/build-arch.sh [--no-plasma]   (--no-plasma: only the first package, no qt needed on this machine)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLASMA=ON; [ "${1:-}" = "--no-plasma" ] && PLASMA=OFF
command -v makepkg >/dev/null || { echo "makepkg not found: this one runs on arch"; exit 1; }
VER="$(sed -n 's/^project(chaos_engine VERSION \([0-9.]*\).*/\1/p' "$ROOT/client/CMakeLists.txt")"
WORK="$ROOT/dist/arch"; rm -rf "$WORK"; mkdir -p "$WORK"
tar -C "$ROOT" --exclude=client/build --exclude=client/build-container --exclude=dist --exclude=.git -czf "$WORK/chaos-src.tar.gz" client shared docs packaging
if [ "$PLASMA" = ON ]; then NAMES="'chaos-engine' 'chaos-engine-plasma'"; MK=" 'qt6-declarative' 'plasma-workspace' 'extra-cmake-modules'"; else NAMES="'chaos-engine'"; MK=""; fi
cat > "$WORK/PKGBUILD" <<PKG
pkgbase=chaos-engine
pkgname=($NAMES)
pkgver=$VER
pkgrel=1
pkgdesc="CHAOS: a seeded, non-repeating 3D dimension as your linux wallpaper"
arch=('x86_64')
url="https://chaos-eng.tetstack.dev"
license=('custom')
makedepends=('cmake' 'ninja' 'catch2' 'glm' 'nlohmann-json' 'glfw' 'libsodium' 'openssl' 'curl' 'libx11' 'libxext' 'wayland' 'libglvnd'$MK)
source=("chaos-src.tar.gz")
sha256sums=('SKIP')
build() {
  cmake -S "\$srcdir/client" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCHAOS_PLASMA=$PLASMA
  cmake --build build
}
check() { ./build/chaos_tests; }
package_chaos-engine() {
  pkgdesc="CHAOS: a seeded, non-repeating 3D dimension as your linux wallpaper (the app and the window-less wallpaper mode)"
  depends=('glfw' 'libsodium' 'openssl' 'curl' 'libx11' 'libxext' 'wayland' 'libglvnd')
  optdepends=('chaos-engine-plasma: the kde plasma 6 wallpaper plugin' 'xorg-xwayland: wallpaper mode on x11 under a wayland session' 'blender: import .blend / .fbx / .dae / .step / .usd models')
  DESTDIR="\$pkgdir" cmake --install build
  rm -rf "\$pkgdir/usr/lib/qt6" "\$pkgdir/usr/share/plasma"     # the plugin goes in its own package
}
package_chaos-engine-plasma() {
  pkgdesc="CHAOS as a kde plasma 6 wallpaper type (the plugin for plasmashell)"
  depends=('chaos-engine' 'qt6-declarative' 'plasma-workspace')
  DESTDIR="\$pkgdir" cmake --install build
  find "\$pkgdir/usr" -mindepth 1 -maxdepth 1 ! -name lib ! -name share -exec rm -rf {} +
  find "\$pkgdir/usr/share" -mindepth 1 -maxdepth 1 ! -name plasma -exec rm -rf {} +
  find "\$pkgdir/usr/lib" -mindepth 1 -maxdepth 1 ! -name qt6 -exec rm -rf {} +
}
PKG
(cd "$WORK" && makepkg -fs --noconfirm)      # -s: pacman installs any missing build dep (asks for sudo)
cp "$WORK"/*.pkg.tar.zst "$ROOT/dist/"
echo; ls -1 "$ROOT"/dist/*.pkg.tar.zst
