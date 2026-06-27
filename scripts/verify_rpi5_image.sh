#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${1:-$ROOT/dist/dev-openwrt-25.12.4-rpi5-squashfs-factory.img.gz}"
RAW="$(mktemp /tmp/dev-image.XXXXXX.img)"
MOUNT="$(mktemp -d /tmp/dev-root.XXXXXX)"

cleanup() {
	umount "$MOUNT" 2>/dev/null || true
	rm -rf "$MOUNT" "$RAW"
}
trap cleanup EXIT

gzip -t "$IMAGE"
gzip -dc "$IMAGE" >"$RAW"
mount -t squashfs -o ro,loop,offset=75497472 "$RAW" "$MOUNT"

required=(
	usr/bin/tr69d usr/bin/trg usr/bin/trs
	usr/lib/tr69d/transformer.lua etc/transformer/maps/Device.map
	etc/config/tr69 etc/init.d/tr69 etc/init.d/dev-wifi-recovery
	etc/rc.d/S95tr69 etc/rc.d/S99dev-wifi-recovery
	usr/sbin/dev-wifi-recovery etc/uci-defaults/90-dev-rpi5
)
for path in "${required[@]}"; do
	test -e "$MOUNT/$path"
	stat -c '%A %s %n' "$MOUNT/$path"
done

for forbidden in usr/bin/tra usr/bin/trd usr/bin/transformer-api usr/bin/tr069d; do
	test ! -e "$MOUNT/$forbidden"
	echo "ABSENT $forbidden"
done

grep -n \
	-e "band='2g'" -e "htmode='HT20'" \
	-e "ssid='Khuynh 2'" -e "ssid='Wifi_cua_Phu'" \
	-e "key='888888888'" -e "key='16022003'" \
	-e "encryption='psk2'" -e "dev_sta.disabled='1'" \
	"$MOUNT/etc/uci-defaults/90-dev-rpi5"

grep -n \
	-e parameter_key -e defaultactivenotificationthrottle \
	-e retry_minimum_wait -e 'option port' \
	-e ConnectionRequestURL -e ConnectionRequestAuthentication \
	-e PeriodicInformTime -e "option url ''" -e 'secret = true' \
	"$MOUNT/etc/config/tr69" "$MOUNT/etc/transformer/maps/Device.map"

file "$MOUNT/usr/bin/tr69d"
readelf -d "$MOUNT/usr/bin/tr69d" | grep NEEDED
readelf -d "$MOUNT/usr/bin/tr69d" | grep -E 'libubus|libubox'
grep -q "ubus call tr69 reload" "$MOUNT/usr/lib/tr69d/transformer.lua"
grep -q "ubus call tr69 reconnect" "$MOUNT/etc/hotplug.d/iface/90-tr69-wwan"
! grep -R "/etc/init.d/tr69 restart" \
	"$MOUNT/usr/lib/tr69d" "$MOUNT/etc/hotplug.d" "$MOUNT/etc/init.d/tr69"
echo IMAGE_VERIFY_OK
