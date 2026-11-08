#!/usr/bin/env bash
# builds an arch / garuda / manjaro package from this tree, plasma plugin included when the kde dev bits are here.
# no git clone, no github: the tree is tarred into dist/ and makepkg builds that. run it on an arch machine.
# out: dist/chaos-engine-<ver>-1-x86_64.pkg.tar.zst   install with: sudo pacman -U dist/chaos-engine-*.pkg.tar.zst
# usage: packaging/build-arch.sh [--no-plasma]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLASMA=ON; [ "${1:-}" = "--no-plasma" ] && PLASMA=OFF
command -v makepkg >/dev/null || { echo "makepkg not found: this one runs on arch"; exit 1; }
VER="$(sed -n 's/^project(chaos_engine VERSION \([0-9.]*\).*/\1/p' "$ROOT/client/CMakeLists.txt")"
WORK="$ROOT/dist/arch"; rm -rf "$WORK"; mkdir -p "$WORK"
tar -C "$ROOT" --exclude=client/build --exclude=client/build-container --exclude=dist --exclude=.git -czf "$WORK/chaos-src.tar.gz" client shared docs packaging
cat > "$WORK/PKGBUILD" <<PKG
pkgname=chaos-engine
pkgver=$VER
pkgrel=1
pkgdesc="CHAOS: a seeded, non-repeating 3D dimension as your linux wallpaper"
arch=('x86_64')
url="https://chaos-eng.tetstack.dev"
license=('custom')
depends=('glfw' 'libsodium' 'openssl' 'curl' 'libx11' 'libxext' 'wayland' 'libglvnd'$([ "$PLASMA" = ON ] && printf " 'qt6-declarative' 'plasma-workspace'"))
makedepends=('cmake' 'ninja' 'catch2' 'glm' 'nlohmann-json'$([ "$PLASMA" = ON ] && printf " 'extra-cmake-modules'"))
optdepends=('xorg-xwayland: wallpaper mode on x11 under a wayland session' 'blender: import .blend / .fbx / .dae / .step / .usd models')
source=("chaos-src.tar.gz")
sha256sums=('SKIP')
build() {
  cmake -S "\$srcdir/client" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCHAOS_PLASMA=$PLASMA
  cmake --build build
}
check() { ./build/chaos_tests; }
package() { DESTDIR="\$pkgdir" cmake --install build; }
PKG
(cd "$WORK" && makepkg -fs --noconfirm)      # -s: pacman installs any missing build dep (asks for sudo)
cp "$WORK"/*.pkg.tar.zst "$ROOT/dist/"
echo; echo "-> $(ls "$ROOT"/dist/*.pkg.tar.zst)"
