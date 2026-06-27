#!/usr/bin/env bash
set -euo pipefail

PI_HOST="${1:-root@192.168.1.1}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${2:-$PROJECT_ROOT/dist/tr69d}"

[[ -f "$BINARY" ]] || { echo "Binary not found: $BINARY" >&2; exit 1; }

scp "$BINARY" "$PI_HOST:/tmp/tr69d"
scp "$PROJECT_ROOT/openwrt/files/etc/config/tr69" "$PI_HOST:/tmp/tr69.config"
scp "$PROJECT_ROOT/openwrt/files/etc/init.d/tr69" "$PI_HOST:/tmp/tr69.init"
scp "$PROJECT_ROOT/transformer/transformer.lua" "$PI_HOST:/tmp/transformer.lua"
scp "$PROJECT_ROOT/transformer/cli.lua" "$PI_HOST:/tmp/transformer-cli.lua"
scp "$PROJECT_ROOT/transformer/maps/Device.map" "$PI_HOST:/tmp/Device.map"
scp "$PROJECT_ROOT/openwrt/files/usr/bin/trg" "$PI_HOST:/tmp/trg"
scp "$PROJECT_ROOT/openwrt/files/usr/bin/trs" "$PI_HOST:/tmp/trs"

ssh "$PI_HOST" 'set -e
  /etc/init.d/tr69 stop 2>/dev/null || true
  install -m 0755 /tmp/tr69d /usr/bin/tr69d
  test -f /etc/config/tr69 || install -m 0600 /tmp/tr69.config /etc/config/tr69
  install -d -m 0755 /usr/lib/tr69d /etc/transformer/maps
  install -m 0644 /tmp/transformer.lua /usr/lib/tr69d/transformer.lua
  install -m 0644 /tmp/transformer-cli.lua /usr/lib/tr69d/cli.lua
  install -m 0644 /tmp/Device.map /etc/transformer/maps/Device.map
  install -m 0755 /tmp/trg /usr/bin/trg
  install -m 0755 /tmp/trs /usr/bin/trs
  install -m 0755 /tmp/tr69.init /etc/init.d/tr69
  /etc/init.d/tr69 enable
  /etc/init.d/tr69 restart
  sleep 2
  logread -e tr69d | tail -n 30 || true'
