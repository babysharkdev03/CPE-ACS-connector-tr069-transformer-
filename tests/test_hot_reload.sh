#!/usr/bin/env bash
set -euo pipefail

BINARY="$1"
SOURCE_DIR="$2"
TEMP_DIR="$(mktemp -d /tmp/tr69d-hot-reload-XXXXXX)"
ACS_PID=""
DAEMON_PID=""
cleanup() {
    [[ -z "$DAEMON_PID" ]] || kill -TERM "$DAEMON_PID" 2>/dev/null || true
    [[ -z "$ACS_PID" ]] || kill -TERM "$ACS_PID" 2>/dev/null || true
    wait "$DAEMON_PID" 2>/dev/null || true
    wait "$ACS_PID" 2>/dev/null || true
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

mkdir -p "$TEMP_DIR/bin"
cp "$SOURCE_DIR/tests/fake_uci.sh" "$TEMP_DIR/bin/uci"
chmod +x "$TEMP_DIR/bin/uci"
DB="$TEMP_DIR/uci.db"
cat >"$DB" <<'EOF'
tr69.mgmt_srv.enable=true
tr69.mgmt_srv.url=http://127.0.0.1:18080/
tr69.mgmt_srv.username=
tr69.mgmt_srv.password=
tr69.mgmt_srv.retry_minimum_wait=5
tr69.mgmt_srv.retry_multiplier=2000
tr69.mgmt_srv.upgrades_managed=false
tr69.mgmt_srv.defaultactivenotificationthrottle=0
tr69.mgmt_srv.parameter_key=
tr69.settings.ssl_ca_cert_path=
tr69.settings.ssl_cn_verification=true
tr69.settings.mock_mode=1
tr69.settings.bootstrap_done=0
tr69.settings.pending_reboot_event=0
tr69.settings.notif_polling_interval=1
tr69.settings.bind=127.0.0.1
tr69.settings.port=17548
tr69.settings.interface=wwan
tr69.settings.debug=true
tr69.settings.log_level=debug
tr69.periodic_inform.enable=true
tr69.periodic_inform.interval=300
tr69.periodic_inform.time=1970-01-01T00:00:00Z
tr69.conn_request.enable=true
tr69.conn_request.auth=None
tr69.conn_request.username=cpe
tr69.conn_request.password=secret
EOF

ACS_HOST=127.0.0.1 ACS_PORT=18080 ACS_DATA_FILE="$TEMP_DIR/devices.json" \
    python3 "$SOURCE_DIR/docker/acs-mock/server.py" >"$TEMP_DIR/acs.log" 2>&1 &
ACS_PID=$!
sleep 1

PATH="$TEMP_DIR/bin:$PATH" UCI_TEST_DB="$DB" \
TR69_ALLOW_MOCK_REBOOT=1 \
TR69_TRANSFORMER_SCRIPT="$SOURCE_DIR/transformer/transformer.lua" \
TR69_DEVICE_MAP="$SOURCE_DIR/transformer/maps/Device.map" \
    "$BINARY" >"$TEMP_DIR/daemon.log" 2>&1 &
DAEMON_PID=$!

for _ in {1..20}; do
    [[ -s "$TEMP_DIR/devices.json" ]] && break
    sleep 0.25
done
[[ -s "$TEMP_DIR/devices.json" ]]
INITIAL_COUNT="$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next(iter(d.values()))["informCount"])' "$TEMP_DIR/devices.json")"

PATH="$TEMP_DIR/bin:$PATH" UCI_TEST_DB="$DB" uci -q set tr69.periodic_inform.interval=2
PATH="$TEMP_DIR/bin:$PATH" UCI_TEST_DB="$DB" uci -q commit tr69
kill -HUP "$DAEMON_PID"

for _ in {1..24}; do
    CURRENT_COUNT="$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next(iter(d.values()))["informCount"])' "$TEMP_DIR/devices.json")"
    [[ "$CURRENT_COUNT" -gt "$INITIAL_COUNT" ]] && break
    sleep 0.25
done

kill -0 "$DAEMON_PID"
grep -q "Runtime configuration updated:.*PeriodicInform=enabled/2s" "$TEMP_DIR/daemon.log"
grep -q "4 VALUE CHANGE" "$TEMP_DIR/devices.json"
[[ "${CURRENT_COUNT:-0}" -gt "$INITIAL_COUNT" ]]
