# Target organization

The supported release targets are `esp8266` (Wemos D1 mini) and `esp32` (ESP32 DevKit). Shared settings live in `[common]`; each target environment contains only board, platform, partition, and compiler differences. The exact framework/toolchain pins and the shared-package-cache recovery procedure are documented in [PlatformIO toolchains and package cache](TOOLCHAINS.md).

## ESP32 OTA partitions

The ESP32 bootloader reads the partition table from flash to find application
slots. An A/B OTA layout needs two OTA application partitions (`ota_0` and
`ota_1`) and an OTA data partition (`otadata`). An OTA upload writes the new
image to the inactive application slot, then changes `otadata` so the bootloader
selects it on the next restart. It does not replace the partition table.

For Arduino projects, PlatformIO uses `default.csv` unless the build
environment sets `board_build.partitions` to another CSV. That setting chooses
the partition-table binary included in a serial/USB upload. A board must
already have an OTA-capable table from its first USB/serial install (or factory
provisioning) before normal OTA application uploads can work. Replacing a
partition table later also requires a serial/recovery flash and can invalidate
an application or data partition whose offset or size changed.

Arduino-ESP32's `default.csv` is OTA-capable, with two `0x140000` (1.25 MiB)
application slots and a SPIFFS partition. The web-enabled DeviceFramework
ESP32 test image needs a larger slot, so the maintained ESP32 environments use
[`partitions/esp32_ota_4m_no_fs.csv`](../partitions/esp32_ota_4m_no_fs.csv): a
4 MB layout with two `0x1F0000` (~1.94 MiB) application slots and no
filesystem. Its web assets are compiled into the image, so the test firmware
does not need SPIFFS.

`huge_app.csv` is an Arduino-ESP32 supplied single-app layout. It has one
large `app0` partition but no inactive `ota_*` partition, so Arduino OTA cannot
perform an A/B update. Use it only when a device intentionally accepts USB-only
application updates.

Consumer firmware should copy the maintained CSV into its own project (or
provide an equivalent board-specific layout) and set:

```ini
board_build.partitions = partitions_ota.csv
```

An interrupted transfer leaves the selected image unchanged. After a successful
transfer, Arduino OTA selects the newly written slot for the next boot. This
layout is not an application-health or automatic-rollback guarantee: standard
Arduino OTA does not prove that the new firmware will run correctly after boot.

To add another board family, add a separate `[env:<family>_<board>]` extending `[common]`, pin its PlatformIO platform version, and add a compile-only CI matrix entry. Do not copy dependencies or machine-specific paths into the new environment. Board-specific partitions and framework workarounds belong only in that environment.

The supported target list in [compatibility](COMPATIBILITY.md) is the released-contract view; this guide explains the development layout.

Validate target changes with [Testing](TESTING.md), including the complete examples on both released target families.

Back to [documentation](README.md) · [project overview](../README.md).
