# Build and deploy tr69d for OpenWrt

## 1. Identify the exact target

Run on the Raspberry Pi before downloading an SDK:

```sh
cat /etc/openwrt_release
opkg print-architecture
uname -m
```

Match the OpenWrt release, target, subtarget and package architecture exactly.

## 2. Prepare the SDK

On the Linux build PC, extract the matching OpenWrt SDK and install its documented host prerequisites. Dependencies are `libcurl`, `libxml2`, `liblua`, `lua`, `libstdcpp`, and `libpthread`.

```sh
cd /path/to/openwrt-sdk
./scripts/feeds update -a
./scripts/feeds install -a
```

Do not compile with the PC's native `g++`; an x86_64 binary cannot run on the Pi. The project script installs the package recipe into the SDK, invokes the SDK toolchain, and collects both the test-friendly binary and an OpenWrt `.apk`/`.ipk` package:

```sh
cd /path/to/Mock_Tr69d
chmod +x scripts/*.sh scripts/genieacs_cli.py
./scripts/build_sdk.sh /path/to/openwrt-sdk clean
ls -l dist/
```

## 3. Fast binary deployment

Install runtime libraries first:

```sh
ssh root@192.168.1.1 'opkg update && opkg install libcurl libxml2 libstdcpp ubus libubus libubox'
./scripts/deploy_to_pi.sh root@192.168.1.1 dist/tr69d
```

The deploy script preserves existing UCI configs, so edit them on the Pi:

```sh
uci set tr69.mgmt_srv.url='http://192.168.1.2:7547/'
uci commit tr69
ubus call tr69 reload '{}'
logread -f -e tr69d
```

For HTTPS with a private CA, copy the CA certificate to the Pi and set `tr69.settings.ssl_ca_cert_path`.

## 4. Install the package

After the binary flow works, install the generated package:

```sh
scp dist/tr69d-*.apk root@192.168.1.1:/tmp/
ssh root@192.168.1.1 'apk add --allow-untrusted /tmp/tr69d-*.apk && /etc/init.d/tr69 enable && /etc/init.d/tr69 start'
```

`/etc/config/tr69` and `/etc/transformer/maps/Device.map` are conffiles.

## 5. Host unit tests

On a Linux host with CMake, a C++17 compiler, libcurl development files, libxml2 development files, and pkg-config:

```sh
./scripts/run_test.sh
```

The tests currently cover state transition guards, SOAP parsing and the internal MD5 implementation used for Digest Connection Request authentication.

## Troubleshooting

- `Exec format error`: SDK target does not match the Pi architecture.
- CMake cannot find libcurl/libxml2: install the feeds and ensure the recipe dependencies were selected.
- ACS never sees Inform: test routing/firewall from Pi to PC and confirm port 7547 is published by Docker.
- Connection Request fails: ensure TCP/7547 reaches the Pi, the advertised URL is correct, and GenieACS has the same username/password.
- HTTPS fails: keep verification enabled and provide the correct CA; inspect the clock on both hosts.
