#!/usr/bin/env bash
# builds CHAOS-x86_64.AppImage from a configured client/build. needs curl. no fuse needed (tools run extracted).
# usage: packaging/build-appimage.sh [build-dir] [out-dir]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/client/build}"
OUT="${2:-$ROOT/dist}"
TOOLS="$ROOT/dist/tools"
mkdir -p "$OUT" "$TOOLS"
APPDIR="$OUT/AppDir"; rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD" --prefix /usr
fetch() { [ -x "$TOOLS/$1" ] || { curl -sSL -o "$TOOLS/$1" "$2"; chmod +x "$TOOLS/$1"; }; }
fetch linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch appimagetool-x86_64.AppImage https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
run() { local t="$1"; shift; if [ -d "$TOOLS/$t.extracted" ]; then "$TOOLS/$t.extracted/AppRun" "$@"; else (cd "$TOOLS" && "./$t" --appimage-extract >/dev/null && mv squashfs-root "$t.extracted") && "$TOOLS/$t.extracted/AppRun" "$@"; fi; }
# bundle the shared libs (glfw, sodium, curl, ssl, x11) next to the binary; leave gl/mesa to the host like every appimage does
run linuxdeploy-x86_64.AppImage --appdir "$APPDIR" --desktop-file "$ROOT/packaging/chaos.desktop" --icon-file "$ROOT/packaging/chaos.svg" --executable "$APPDIR/usr/bin/chaos"
cat > "$APPDIR/AppRun" <<'RUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH:-}"
exec "$HERE/usr/bin/chaos" "$@"
RUN
chmod +x "$APPDIR/AppRun"
ARCH=x86_64 run appimagetool-x86_64.AppImage "$APPDIR" "$OUT/CHAOS-x86_64.AppImage"
echo "-> $OUT/CHAOS-x86_64.AppImage"
