#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 /path/to/openwrt-sdk [clean]" >&2
  exit 2
}

[[ $# -ge 1 ]] || usage
SDK_DIR="$(cd "$1" && pwd)"
MODE="${2:-}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE_DIR="$SDK_DIR/package/tr69d"
DIST_DIR="$PROJECT_ROOT/dist"
JOBS="${JOBS:-$(nproc)}"

mkdir -p "$PACKAGE_DIR" "$DIST_DIR"
cp "$PROJECT_ROOT/openwrt/Makefile" "$PACKAGE_DIR/Makefile"

if [[ -x "$SDK_DIR/scripts/feeds" &&
      ( ! -e "$SDK_DIR/package/feeds/base/ubus" ||
        ! -e "$SDK_DIR/package/feeds/base/libubox" ) ]]; then
  (
    cd "$SDK_DIR"
    ./scripts/feeds update base
    ./scripts/feeds install ubus libubox
  )
fi

make -C "$SDK_DIR" defconfig
if [[ "$MODE" == "clean" ]]; then
  make -C "$SDK_DIR" package/tr69d/clean TR69D_PROJECT_ROOT="$PROJECT_ROOT"
fi
make -C "$SDK_DIR" -j"$JOBS" package/tr69d/compile V=s TR69D_PROJECT_ROOT="$PROJECT_ROOT"

find "$SDK_DIR/bin/packages" -type f \( -name 'tr69d_*.ipk' -o -name 'tr69d-*.apk' \) \
  -exec cp -v {} "$DIST_DIR/" \;
BINARY="$(find "$SDK_DIR/build_dir" -type f \( \
  -path '*/tr69d-*/ipkg-install/usr/bin/tr69d' -o \
  -path '*/tr69d-*/.pkgdir/tr69d/usr/bin/tr69d' \) -print -quit)"
if [[ -n "$BINARY" ]]; then
  cp -v "$BINARY" "$DIST_DIR/tr69d"
fi
echo "Artifacts: $DIST_DIR"
