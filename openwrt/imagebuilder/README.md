# Raspberry Pi 5 image overlay

This overlay is consumed by `scripts/build_rpi5_image.sh`. It configures:

- Raspberry Pi 5 profile (`bcm27xx/bcm2712`, OpenWrt 25.12.4)
- LAN and LuCI at `https://192.168.8.1/`
- an encrypted `Wifi_cua_Phu` access point
- a 2.4 GHz WPA2 AP-first setup and a managed station for `Khuynh 2`
- NAT from LAN/AP clients through the `wwan` station interface
- `tr69d` and the Lua Transformer enabled at boot

The recovery service starts AP-only, attempts the uplink, then restores AP-only
if association fails. To change it later:

```sh
dev-wifi-uplink 'UPSTREAM_SSID' 'UPSTREAM_PASSWORD' VN 2g
```

The onboard radio can expose AP and STA interfaces simultaneously, but both must use the same channel. For the most reliable router/repeater deployment, use the onboard radio for AP and a supported USB Wi-Fi adapter for the uplink.
