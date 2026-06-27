#!/usr/bin/env bash
set -euo pipefail

# Avoid Windows PATH fragments such as "Program Files" breaking GNU find -execdir in WSL.
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

OPENWRT_VERSION="${OPENWRT_VERSION:-25.12.4}"
TARGET="bcm27xx"
SUBTARGET="bcm2712"
PROFILE="rpi-5"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${WORK_DIR:-}" ]]; then
	# OpenWrt requires a case-sensitive filesystem. WSL paths under /mnt/c are
	# backed by NTFS, so keep the SDK/ImageBuilder in the Linux home directory.
	if [[ "$PROJECT_ROOT" == /mnt/* ]]; then
		WORK_DIR="$HOME/.cache/dev-openwrt/$OPENWRT_VERSION-$TARGET-$SUBTARGET"
	else
		WORK_DIR="$PROJECT_ROOT/.openwrt-build"
	fi
fi
CACHE_DIR="$WORK_DIR/cache"
DIST_DIR="$PROJECT_ROOT/dist"
OVERLAY_DIR="$WORK_DIR/overlay-rpi5"
BASE_URL="https://downloads.openwrt.org/releases/$OPENWRT_VERSION/targets/$TARGET/$SUBTARGET"
SDK_ARCHIVE="openwrt-sdk-${OPENWRT_VERSION}-${TARGET}-${SUBTARGET}_gcc-14.3.0_musl.Linux-x86_64.tar.zst"
IB_ARCHIVE="openwrt-imagebuilder-${OPENWRT_VERSION}-${TARGET}-${SUBTARGET}.Linux-x86_64.tar.zst"

AP_SSID="${AP_SSID:-Wifi_cua_Phu}"
AP_KEY="${AP_KEY:-16022003}"
STA_SSID="${STA_SSID:-Khuynh 2}"
STA_KEY="${STA_KEY:-888888888}"
ROOT_PASSWORD="${ROOT_PASSWORD:-1}"
WIFI_COUNTRY="${WIFI_COUNTRY:-VN}"
LAN_IP="${LAN_IP:-192.168.8.1}"
ROOT_HASH="$(openssl passwd -6 "$ROOT_PASSWORD")"

mkdir -p "$CACHE_DIR" "$DIST_DIR"

download() {
	local name="$1"
	if [[ ! -s "$CACHE_DIR/$name" ]]; then
		curl -fL --retry 3 -o "$CACHE_DIR/$name" "$BASE_URL/$name"
	fi
}

extract_archive() {
	local archive="$1"
	local marker="$2"
	[[ -d "$WORK_DIR/$marker" ]] && return 0
	if command -v unzstd >/dev/null 2>&1; then
		tar --use-compress-program=unzstd -xf "$CACHE_DIR/$archive" -C "$WORK_DIR"
	elif command -v zstd >/dev/null 2>&1; then
		tar --use-compress-program='zstd -d' -xf "$CACHE_DIR/$archive" -C "$WORK_DIR"
	else
		echo "zstd is required to extract $archive" >&2
		exit 1
	fi
}

download "$SDK_ARCHIVE"
download "$IB_ARCHIVE"
curl -fL --retry 3 -o "$CACHE_DIR/sha256sums" "$BASE_URL/sha256sums"
(
	cd "$CACHE_DIR"
	grep "$SDK_ARCHIVE\$" sha256sums | sha256sum -c -
	grep "$IB_ARCHIVE\$" sha256sums | sha256sum -c -
)

SDK_DIRNAME="${SDK_ARCHIVE%.tar.zst}"
IB_DIRNAME="${IB_ARCHIVE%.tar.zst}"
extract_archive "$SDK_ARCHIVE" "$SDK_DIRNAME"
extract_archive "$IB_ARCHIVE" "$IB_DIRNAME"
SDK_DIR="$WORK_DIR/$SDK_DIRNAME"
IB_DIR="$WORK_DIR/$IB_DIRNAME"

if [[ ! -e "$SDK_DIR/package/feeds/packages/curl" ||
      ! -e "$SDK_DIR/package/feeds/base/libxml2" ||
      ! -e "$SDK_DIR/package/feeds/base/ubus" ||
      ! -e "$SDK_DIR/package/feeds/base/libubox" ]]; then
	(
		cd "$SDK_DIR"
		./scripts/feeds update packages
		./scripts/feeds update base
		./scripts/feeds install curl libxml2 mbedtls lua ubus libubox
	)
fi

rm -rf "$SDK_DIR/package/tr069d" "$SDK_DIR/package/tr69d"
mkdir -p "$SDK_DIR/package/tr69d"
cp "$PROJECT_ROOT/openwrt/Makefile" "$SDK_DIR/package/tr69d/Makefile"
make -C "$SDK_DIR" defconfig
(
	cd "$SDK_DIR"
	perl scripts/kconfig.pl + .config "$PROJECT_ROOT/openwrt/sdk-rpi5.config" >.config.new
	mv .config.new .config
	make defconfig
)
make -C "$SDK_DIR" package/tr69d/clean TR69D_PROJECT_ROOT="$PROJECT_ROOT"
make -C "$SDK_DIR" -j"${JOBS:-$(nproc)}" package/tr69d/compile V=s \
	TR69D_PROJECT_ROOT="$PROJECT_ROOT"

PACKAGE_FILE="$(find "$SDK_DIR/bin/packages" -type f \( -name 'tr69d_*.ipk' -o -name 'tr69d-*.apk' \) | sort -V | tail -n 1)"
[[ -n "$PACKAGE_FILE" ]] || { echo "tr69d package was not produced" >&2; exit 1; }
cp "$PACKAGE_FILE" "$DIST_DIR/"
BINARY="$(find "$SDK_DIR/build_dir" -type f -path '*/tr69d-*/.pkgdir/tr69d/usr/bin/tr69d' -print -quit)"
[[ -n "$BINARY" ]] && cp "$BINARY" "$DIST_DIR/tr69d"
mkdir -p "$IB_DIR/packages"
rm -f "$IB_DIR"/packages/tr069d*.apk "$IB_DIR"/packages/tr069d*.ipk \
	"$IB_DIR"/packages/tr69d*.apk "$IB_DIR"/packages/tr69d*.ipk
cp "$PACKAGE_FILE" "$IB_DIR/packages/"

rm -rf "$OVERLAY_DIR"
mkdir -p "$OVERLAY_DIR"
cp -a "$PROJECT_ROOT/openwrt/imagebuilder/files/." "$OVERLAY_DIR/"
mkdir -p "$OVERLAY_DIR/etc/rc.d"
ln -sfn ../init.d/tr69 "$OVERLAY_DIR/etc/rc.d/S95tr69"
ln -sfn ../init.d/dev-wifi-recovery \
	"$OVERLAY_DIR/etc/rc.d/S99dev-wifi-recovery"
find "$OVERLAY_DIR" -type f -print0 | xargs -0 sed -i \
	-e "s|@AP_SSID@|$AP_SSID|g" \
	-e "s|@AP_KEY@|$AP_KEY|g" \
	-e "s|@STA_SSID@|$STA_SSID|g" \
	-e "s|@STA_KEY@|$STA_KEY|g" \
	-e "s|@WIFI_COUNTRY@|$WIFI_COUNTRY|g" \
	-e "s|@LAN_IP@|$LAN_IP|g" \
	-e "s|@ROOT_HASH@|$ROOT_HASH|g"
chmod 0755 "$OVERLAY_DIR/etc/uci-defaults/90-dev-rpi5" \
	"$OVERLAY_DIR/etc/hotplug.d/iface/90-tr69-wwan" \
	"$OVERLAY_DIR/etc/init.d/dev-wifi-recovery" \
	"$OVERLAY_DIR/usr/sbin/dev-wifi-recovery" \
	"$OVERLAY_DIR/usr/bin/dev-wifi-uplink" \
	"$OVERLAY_DIR/usr/bin/dev-wifi-uplink-off"

PACKAGES="tr69d lua luci-ssl luci-app-firewall curl ca-bundle iw-full wireless-regdb ip-full tcpdump nano htop avahi-nodbus-daemon avahi-daemon-service-ssh avahi-daemon-service-http -wpad-basic-mbedtls wpad-openssl"
make -C "$IB_DIR" image PROFILE="$PROFILE" PACKAGES="$PACKAGES" \
	FILES="$OVERLAY_DIR" ROOTFS_PARTSIZE=512

IMAGE="$(find "$IB_DIR/bin/targets/$TARGET/$SUBTARGET" -type f \
	-name "*${PROFILE}*squashfs-factory.img.gz" -print -quit)"
[[ -n "$IMAGE" ]] || { echo "Factory image was not produced" >&2; exit 1; }
OUTPUT="$DIST_DIR/dev-openwrt-${OPENWRT_VERSION}-rpi5-squashfs-factory.img.gz"
cp "$IMAGE" "$OUTPUT"
(
	cd "$DIST_DIR"
	sha256sum "$(basename "$OUTPUT")" >"$(basename "$OUTPUT").sha256"
)
cat >"$DIST_DIR/RPI5_IMAGE_CREDENTIALS.txt" <<EOF
Image: $(basename "$OUTPUT")
LAN address: $LAN_IP
LuCI: https://$LAN_IP/
SSH user: root
Root password: $ROOT_PASSWORD
Wi-Fi AP SSID: $AP_SSID
Wi-Fi AP password: $AP_KEY
Wi-Fi uplink SSID: $STA_SSID
Wi-Fi uplink password: $STA_KEY
Wi-Fi country: $WIFI_COUNTRY

The onboard Wi-Fi starts a 2.4 GHz AP first, then automatically attempts the uplink.
If uplink association fails, AP-only fallback is restored. To change the uplink:
  dev-wifi-uplink '<UPSTREAM_SSID>' '<UPSTREAM_PASSWORD>' '$WIFI_COUNTRY' '2g'

SSH from the AP:
  ssh root@$LAN_IP
SSH from the same upstream network:
  ssh root@dev-rpi5.local
  # or use the DHCP address shown by the upstream router
EOF
chmod 0600 "$DIST_DIR/RPI5_IMAGE_CREDENTIALS.txt"

echo "Image ready: $OUTPUT"
echo "Credentials: $DIST_DIR/RPI5_IMAGE_CREDENTIALS.txt"
