# Raspberry Pi 5 image: flash and first boot

## Image

Write `dist/dev-openwrt-25.12.4-rpi5-squashfs-factory.img.gz` directly to a microSD card with Raspberry Pi Imager or balenaEtcher. Do not unzip it first. The SHA-256 is stored beside the image.

The generated login and Wi-Fi details are in `dist/RPI5_IMAGE_CREDENTIALS.txt`.

## Automatic networking

On first boot the Pi automatically:

- connects to uplink SSID `Khuynh 2` using WPA2;
- creates AP `Wifi_cua_Phu` with LAN address `192.168.8.1`;
- runs DHCP/NAT for AP clients through the `wwan` uplink;
- permits SSH, LuCI, mDNS and TR-069 Connection Request from the uplink network;
- advertises SSH/HTTP with Avahi as `dev-rpi5.local`;
- starts `tr69d`; ConnectionRequestURL is computed from the current `wwan` IP.

The AP starts first on 2.4 GHz. STA is then attempted; if it cannot associate,
the recovery service restores AP-only mode so SSH remains available.

## SSH without UART or HDMI

From a host already connected to `Khuynh 2`:

```sh
ssh root@dev-rpi5.local
```

If `.local` resolution is unavailable, find the `dev-rpi5` DHCP lease in the upstream router and SSH to that IP. The deterministic fallback is to connect the host to `Wifi_cua_Phu` and run:

```sh
ssh root@192.168.8.1
```

Use the root password from `dist/RPI5_IMAGE_CREDENTIALS.txt`. LuCI is available at `https://192.168.8.1/` and at the Pi's uplink DHCP address.

## Verify after login

```sh
wifi status
ubus call network.interface.wwan status
pgrep -a tr69d
cat /etc/config/tr69
trg Device.DeviceInfo.SerialNumber
trg Device.ManagementServer.
```

Change uplink credentials later with:

```sh
dev-wifi-uplink 'NEW_SSID' 'NEW_PASSWORD' VN 2g
```

## Connect to GenieACS

Publish Docker's CWMP port `7547` on the PC and use the PC's IP on the `Khuynh 2` network, not `localhost` or a container-only address:

```sh
trs Device.ManagementServer.URL 'http://PC_IP:7547/'
trs Device.ManagementServer.Username admin
trs Device.ManagementServer.Password admin
trs Device.ManagementServer.PeriodicInformInterval 60 demo-key
```

Each ManagementServer write is transactionally persisted to `/etc/config/tr69`.
HTTPS is supported by libcurl. For a private CA, copy its certificate to the Pi and configure:

```sh
uci set tr69.settings.ssl_ca_cert_path='/etc/ssl/certs/my-acs-ca.pem'
uci set tr69.settings.ssl_cn_verification='true'
uci commit tr69
ubus call tr69 reload '{}'
```

The lightweight Transformer files are:

- `/etc/transformer/maps/Device.map`: Lua TR-181 schema and UCI mappings;
- `/usr/lib/tr69d/transformer.lua`: validation and transaction engine;
- `trg`, `trs`: get and set commands.

The current model covers DeviceInfo, ManagementServer, Connection Request and core TR-069 internal settings. Wi-Fi remains managed through standard OpenWrt UCI (`/etc/config/wireless`).
