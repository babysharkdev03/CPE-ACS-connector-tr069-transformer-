# Mock TR-069 CPE for OpenWrt RPi5

`Mock_Tr69d` là hệ thống mô phỏng CPE TR-069/TR-181 chạy trên OpenWrt/Raspberry Pi 5. Thành phần chính là daemon C++ `tr69d`, Lua Transformer, mapping TR-181 sang UCI, CLI `trg`/`trs`, package OpenWrt và pipeline build firmware RPi5.

## Tài Liệu Chính

- [docs/OPENWRT_BUILD_PIPELINE.md](docs/OPENWRT_BUILD_PIPELINE.md): quy trình build OpenWrt từ đầu đến cuối, WSL, SDK, ImageBuilder, cross-compile, package và firmware.
- [docs/SYSTEM_OVERVIEW_FLOW.md](docs/SYSTEM_OVERVIEW_FLOW.md): phân tích flow tổng quan của hệ thống và sơ đồ activity.
- [docs/system-overview.drawio](docs/system-overview.drawio): sơ đồ draw.io có thể import/chỉnh sửa.
- [docs/BUILD_OPENWRT.md](docs/BUILD_OPENWRT.md): ghi chú build/deploy OpenWrt package.
- [docs/GENIEACS_TEST.md](docs/GENIEACS_TEST.md): hướng dẫn test với GenieACS.
- [docs/RPI5_IMAGE.md](docs/RPI5_IMAGE.md): ghi chú image Raspberry Pi 5.
- [docs/DESIGN.md](docs/DESIGN.md): thiết kế kỹ thuật.

## Build Nhanh Firmware RPi5

Chạy trong WSL/Linux:

```sh
scripts/build_rpi5_image.sh
```

Artifact nằm trong:

```text
dist/
```

Các file chính:

```text
dist/tr69d-0.2.4-r8.apk
dist/tr69d
dist/dev-openwrt-25.12.4-rpi5-squashfs-factory.img.gz
dist/dev-openwrt-25.12.4-rpi5-squashfs-factory.img.gz.sha256
dist/RPI5_IMAGE_CREDENTIALS.txt
```

## Build/Test Local

```sh
cmake -S . -B build-live -DBUILD_TESTING=ON
cmake --build build-live
cd build-live
ctest --output-on-failure
```

Host build chỉ dùng để test trên WSL/Linux x86_64. Binary chạy trên Pi phải được build bằng OpenWrt SDK.

