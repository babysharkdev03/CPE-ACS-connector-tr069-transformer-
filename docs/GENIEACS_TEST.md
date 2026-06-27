# GenieACS integration test

## Network assumptions

- GenieACS CWMP: `http://PC_IP:7547`
- GenieACS NBI: `http://PC_IP:7557`
- CPE Connection Request: `http://PI_IP:7547/`
- PC and Pi routes/firewalls allow both directions.

Expose the CWMP and NBI ports from the existing GenieACS Docker deployment. Configure the Pi with the PC's LAN IP, not `127.0.0.1` and not a Docker-only container address.

If the automatically computed Connection Request URL is wrong, override it locally:

```sh
uci set tr69.conn_request.url='http://PI_IP:7547/connection-request'
uci commit tr69
ubus call tr69 reload '{}'
ubus call tr69 inform_now '{}'
```

## 1. First Inform

```sh
ubus call tr69 inform_now '{}'
logread -f -e tr69d
```

The first successful session contains `0 BOOTSTRAP` and `1 BOOT`; later daemon starts contain `1 BOOT`. Verify the device appears in GenieACS by serial number.

## 2. PC CLI examples

```sh
CLI="python3 scripts/genieacs_cli.py --nbi http://127.0.0.1:7557"
$CLI list-devices

$CLI get-parameter 'dev-000000-OpenWrt-RPi3-RPI3-DEMO-001' \
  Device.DeviceInfo.SoftwareVersion

$CLI set-parameter 'dev-000000-OpenWrt-RPi3-RPI3-DEMO-001' \
  Device.ManagementServer.PeriodicInformInterval 120 --type xsd:unsignedInt

$CLI reboot 'dev-000000-OpenWrt-RPi3-RPI3-DEMO-001'
$CLI task-status TASK_ID
```

The exact GenieACS device ID is returned by `list-devices`; copy it rather than assuming the example ID.

## 3. Verify SetParameterValues persistence

On the Pi:

```sh
uci get tr69.periodic_inform.interval
cat /etc/config/tr69
```

Read-only paths return a CWMP parameter fault. Password paths accept writes but intentionally return an empty string on reads.

## 4. Connection Request

Ensure GenieACS learned these Inform parameters:

- `Device.ManagementServer.ConnectionRequestURL`
- `Device.ManagementServer.ConnectionRequestUsername`
- `Device.ManagementServer.ConnectionRequestPassword`

`Device.ManagementServer.ConnectionRequestURL` must point to the Pi, not to the
GenieACS host. Example: if GenieACS is `http://192.168.88.72:7547/`, the
Connection Request URL should look like `http://192.168.88.x:7547/connection-request`, where
`192.168.88.x` is the Pi's current `wwan` IP. If it becomes `127.0.0.1`, GenieACS
will call itself and commonly shows `Unexpected status code 405`.

The listener follows `tr69.conn_request.auth`: with `None`, no auth is required;
with `Digest`, use Digest MD5 credentials. A successful request wakes the daemon
and produces event `6 CONNECTION REQUEST`. For direct diagnosis:

```sh
curl http://PI_IP:7547/connection-request -v
curl --digest -u cpe:change-me http://PI_IP:7547/connection-request -v
```

## 5. Reboot

Keep `tr69.settings.mock_mode=1` first. A task should receive `RebootResponse` and the Pi log should show a mock reboot. For a real test:

```sh
uci set tr69.settings.mock_mode='0'
uci commit tr69
ubus call tr69 reload '{}'
```

After the task, verify the Pi restarts and sends `1 BOOT`.

## 6. Download and TransferComplete

First upload a file to GenieACS's file store. A typical NBI upload is:

```sh
curl -X PUT 'http://127.0.0.1:7557/files/demo.bin' \
  --data-binary @demo.bin \
  -H 'fileType: 1 Firmware Upgrade Image' \
  -H 'oui: 000000' \
  -H 'productClass: OpenWrt-RPi3' \
  -H 'version: demo-1'
```

Then queue the task:

```sh
$CLI download-file DEVICE_ID demo.bin --file-type '1 Firmware Upgrade Image'
$CLI task-status TASK_ID
```

Verify a file appears under `/tmp/tr069-downloads/` and GenieACS receives TransferComplete. The mock daemon downloads but does not flash or execute firmware.

## 7. Expected task lifecycle

The NBI POST returns a task JSON document containing `_id`. `task-status` shows the queued/completed/faulted record while it remains in the task collection. GenieACS may remove a completed task quickly depending on version/configuration, so also check the device fault/task UI and CPE logs.

## Known demo limits

Only exact supported parameter paths are accepted; object-prefix expansion is not implemented. The daemon uses CWMP 1.2 namespace, one envelope, HTTP Digest MD5 for Connection Request, and a single-threaded ACS session. Production work should add retry backoff, replay-resistant nonce lifetime checks, interface/IP change handling, firmware verification, download application, richer faults, and full interoperability tests.
