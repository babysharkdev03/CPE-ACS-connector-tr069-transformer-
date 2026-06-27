#!/usr/bin/env bash
set -euo pipefail

ACS_HOST="${1:-172.27.74.146}"
DEVICE="${2:-000000-OpenWrt%2DRPi5-UNKNOWN}"
CWMP="http://${ACS_HOST}:7547/"
NBI="http://${ACS_HOST}:7557"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_DIR="$(mktemp -d /tmp/tr69d-genieacs-live-XXXXXX)"
DAEMON_PID=""

cleanup() {
    if [[ -n "$DAEMON_PID" ]]; then
        kill -TERM "$DAEMON_PID" 2>/dev/null || true
        wait "$DAEMON_PID" 2>/dev/null || true
    fi
    echo "TEMP_DIR=$TEMP_DIR"
}
trap cleanup EXIT

mkdir -p "$TEMP_DIR/bin"
cat >"$TEMP_DIR/bin/uci" <<EOF
#!/bin/sh
exec "$SOURCE_DIR/tests/fake_uci.sh" "\$@"
EOF
chmod +x "$TEMP_DIR/bin/uci"
cat >"$TEMP_DIR/bin/ubus" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$TEMP_DIR/bin/ubus"

DB="$TEMP_DIR/uci.db"
cat >"$DB" <<EOF
tr69.mgmt_srv.enable=true
tr69.mgmt_srv.url=$CWMP
tr69.mgmt_srv.username=
tr69.mgmt_srv.password=
tr69.mgmt_srv.retry_minimum_wait=1
tr69.mgmt_srv.retry_multiplier=1000
tr69.mgmt_srv.upgrades_managed=false
tr69.mgmt_srv.defaultactivenotificationthrottle=0
tr69.mgmt_srv.parameter_key=
tr69.settings.ssl_ca_cert_path=
tr69.settings.ssl_cn_verification=true
tr69.settings.mock_mode=1
tr69.settings.bootstrap_done=0
tr69.settings.pending_reboot_event=0
tr69.settings.notif_polling_interval=1
tr69.settings.bind=0.0.0.0
tr69.settings.port=17549
tr69.settings.interface=wwan
tr69.settings.debug=true
tr69.settings.log_level=debug
tr69.periodic_inform.enable=true
tr69.periodic_inform.interval=3
tr69.periodic_inform.time=1970-01-01T00:00:00Z
tr69.conn_request.enable=true
tr69.conn_request.auth=None
tr69.conn_request.username=cpe
tr69.conn_request.password=change-me
EOF

TR69D_BINARY="${TR69D_BINARY:-$SOURCE_DIR/build-live/tr69d}"

PATH="$TEMP_DIR/bin:$PATH" UCI_TEST_DB="$DB" \
TR69_ALLOW_MOCK_REBOOT=1 \
TR69_TRANSFORMER_SCRIPT="$SOURCE_DIR/transformer/transformer.lua" \
TR69_DEVICE_MAP="$SOURCE_DIR/transformer/maps/Device.map" \
    "$TR69D_BINARY" >"$TEMP_DIR/daemon.log" 2>&1 &
DAEMON_PID=$!

sleep 1
curl -sS -m 3 -i "http://${ACS_HOST}:17549/connection-request" | sed -n '1,8p' || true

python3 - "$NBI" "$DEVICE" "$DB" "$TEMP_DIR" "$DAEMON_PID" <<'PY'
import json
import os
import subprocess
import sys
import time
import urllib.parse
import urllib.request

nbi, device, db, temp, daemon_pid = sys.argv[1:6]
bin_path = os.path.join(temp, "bin")


def request(method, path, payload=None, timeout=20):
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(nbi + path, data=data, method=method)
    req.add_header("Accept", "application/json")
    if data is not None:
        req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=timeout) as response:
        raw = response.read().decode()
        return json.loads(raw) if raw else {"status": response.status}


def devices():
    query = urllib.parse.urlencode({
        "query": json.dumps({"_id": device}, separators=(",", ":"))
    })
    return request("GET", "/devices/?" + query)


def current_value():
    records = devices()
    if not records:
        return None
    return (
        records[0]
        .get("Device", {})
        .get("ManagementServer", {})
        .get("PeriodicInformInterval", {})
        .get("_value")
    )


def connection_request_url():
    records = devices()
    if not records:
        return None
    return (
        records[0]
        .get("Device", {})
        .get("ManagementServer", {})
        .get("ConnectionRequestURL", {})
        .get("_value")
    )


def last_inform():
    records = devices()
    return records[0].get("_lastInform") if records else None


def uci_env():
    env = dict(os.environ)
    env["PATH"] = bin_path + ":" + env.get("PATH", "")
    env["UCI_TEST_DB"] = db
    return env


def cpe_value():
    return cpe_uci("tr69.periodic_inform.interval")


def cpe_uci(key):
    return subprocess.check_output(
        ["uci", "-q", "get", key],
        env=uci_env(),
        text=True,
    ).strip()


def task(payload, connection_request=True):
    suffix = "?connection_request" if connection_request else ""
    return request(
        "POST",
        "/devices/" + urllib.parse.quote(device, safe="") + "/tasks" + suffix,
        payload,
    )


def wait_value(expected, seconds=18):
    expected = str(expected)
    deadline = time.time() + seconds
    latest = None
    while time.time() < deadline:
        latest = current_value()
        if str(latest) == expected:
            return True, latest
        time.sleep(0.5)
    return False, latest


def wait_cpe_uci(key, expected, seconds=18):
    expected = str(expected)
    deadline = time.time() + seconds
    latest = None
    while time.time() < deadline:
        try:
            latest = cpe_uci(key)
        except subprocess.CalledProcessError:
            latest = None
        if latest == expected:
            return True, latest
        time.sleep(0.5)
    return False, latest


def wait_inform_change(previous, seconds=18):
    deadline = time.time() + seconds
    latest = previous
    while time.time() < deadline:
        latest = last_inform()
        if latest and latest != previous:
            return True, latest
        time.sleep(0.5)
    return False, latest


for _ in range(30):
    if devices() and last_inform():
        break
    time.sleep(0.5)
else:
    raise SystemExit("device did not appear in GenieACS")

print("initial", {
    "acs_value": current_value(),
    "cpe_value": cpe_value(),
    "connectionRequestUrl": connection_request_url(),
    "lastInform": last_inform(),
}, flush=True)

results = []

acs_set_cases = [
    ("Device.ManagementServer.PeriodicInformInterval", "tr69.periodic_inform.interval", "xsd:unsignedInt", [109, 110, 111, 112]),
    ("Device.ManagementServer.CWMPRetryMinimumWaitInterval", "tr69.mgmt_srv.retry_minimum_wait", "xsd:unsignedInt", [13, 14, 15, 16]),
    ("Device.ManagementServer.CWMPRetryIntervalMultiplier", "tr69.mgmt_srv.retry_multiplier", "xsd:unsignedInt", [2421, 2422, 2423, 2424]),
    ("Device.ManagementServer.DefaultActiveNotificationThrottle", "tr69.mgmt_srv.defaultactivenotificationthrottle", "xsd:unsignedInt", [21, 22, 23, 24]),
]

for name, uci_key, xsd_type, values in acs_set_cases:
  for value in values:
    response = task({
        "name": "setParameterValues",
        "parameterValues": [[
            name,
            str(value),
            xsd_type,
        ]],
    })
    if name == "Device.ManagementServer.PeriodicInformInterval":
        ok, seen = wait_value(value)
        cpe = cpe_uci(uci_key)
        if not ok or cpe != str(value):
            raise SystemExit(f"acs_set failed parameter={name} value={value} acs={seen} cpe={cpe} task={response}")
    else:
        ok, cpe = wait_cpe_uci(uci_key, value)
        seen = current_value()
        if cpe != str(value):
            raise SystemExit(f"acs_set failed parameter={name} value={value} cpe={cpe} task={response}")
    results.append(("acs_set", name, value, ok, seen, cpe, response.get("_id")))

for value in range(209, 221):
    before = last_inform()
    subprocess.check_call(["uci", "-q", "set", f"tr69.periodic_inform.interval={value}"], env=uci_env())
    subprocess.check_call(["uci", "-q", "commit", "tr69"], env=uci_env())
    subprocess.check_call(["kill", "-HUP", daemon_pid])
    changed, latest = wait_inform_change(before)
    ok, seen = wait_value(value)
    cpe = cpe_value()
    results.append(("cpe_to_acs", value, changed and ok, seen, cpe, latest))
    if not (changed and ok) or cpe != str(value):
        raise SystemExit(f"cpe_to_acs failed value={value} acs={seen} cpe={cpe} inform={latest}")

for index in range(1, 13):
    before = last_inform()
    response = task({
        "name": "getParameterValues",
        "parameterNames": ["Device.ManagementServer.PeriodicInformInterval"],
    })
    changed, latest = wait_inform_change(before)
    seen = current_value()
    cpe = cpe_value()
    results.append(("summon_get", index, changed, seen, cpe, connection_request_url(), response.get("_id")))
    if not changed or str(seen) != cpe:
        raise SystemExit(f"summon_get failed i={index} acs={seen} cpe={cpe} task={response}")

for index in range(1, 13):
    before = last_inform()
    response = task({"name": "reboot"})
    changed, latest = wait_inform_change(before, seconds=20)
    results.append(("reboot", index, changed, current_value(), cpe_value(), connection_request_url(), response.get("_id")))
    if not changed:
        raise SystemExit(f"reboot failed i={index} task={response} latest={latest}")

print("RESULTS", flush=True)
for row in results:
    print(row, flush=True)

print("LOG_TAIL", flush=True)
with open(os.path.join(temp, "daemon.log"), encoding="utf-8", errors="replace") as handle:
    print(handle.read()[-6000:], flush=True)
PY
