#!/usr/bin/env bash
# builds the tester packages inside a debian bookworm container (glibc 2.36), so the same binaries run on kali,
# ubuntu 22.04+, fedora, arch and friends. needs podman or docker on this machine. nothing else is touched.
# out: dist/chaos-engine_<ver>_amd64.deb, dist/chaos-engine-<ver>.x86_64.rpm, dist/chaos-engine-<ver>-Linux.tar.gz,
#      dist/CHAOS-x86_64.AppImage. no plasma plugin in these (that one is built per distro, see packaging/build-arch.sh
#      and the readme): the app and `chaos --wallpaper` are in.
# usage: packaging/build-portable.sh            BASE=debian:trixie packaging/build-portable.sh  (newer gcc, newer glibc)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="${BASE:-debian:bookworm}"
ENGINE="${1:-}"
if [ -z "$ENGINE" ]; then command -v podman >/dev/null && ENGINE=podman || ENGINE=docker; fi
command -v "$ENGINE" >/dev/null || { echo "needs podman or docker (sudo pacman -S podman / sudo apt install podman)"; exit 1; }
mkdir -p "$ROOT/dist"
# the whole build happens inside; the repo is mounted read-only, output goes to dist/
"$ENGINE" run --rm -v "$ROOT:/src:ro" -v "$ROOT/dist:/out" -e DEBIAN_FRONTEND=noninteractive -e BASE="$BASE" "$BASE" bash -euo pipefail -c '
  apt-get update -qq
  apt-get install -qq -y --no-install-recommends build-essential cmake ninja-build pkg-config git curl ca-certificates file rpm \
    libglfw3-dev libglm-dev libsodium-dev libssl-dev libcurl4-openssl-dev catch2 nlohmann-json3-dev libwayland-dev libegl-dev \
    libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libgl-dev libfreetype-dev libsecret-1-dev libargon2-dev libspdlog-dev >/dev/null
  cp -r /src /work && cd /work
  rm -rf client/build client/build-container
  cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCHAOS_PLASMA=OFF >/dev/null
  cmake --build client/build
  if [ -x client/build/chaos_tests ]; then ./client/build/chaos_tests; else echo "tests skipped: no catch2 3 on $BASE, they ran on the host build"; fi
  (cd client/build && cpack -G "DEB;RPM;TGZ" >/dev/null && ls *.deb *.rpm *.tar.gz)
  cp client/build/*.deb client/build/*.rpm client/build/*.tar.gz /out/
  packaging/build-appimage.sh client/build /out
  chown -R "$(stat -c %u:%g /out)" /out || true
'
echo
echo "in $ROOT/dist:"; ls -1 "$ROOT/dist" | grep -v "^AppDir$\|^tools$"
