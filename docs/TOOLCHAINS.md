# PlatformIO toolchains and cache

DeviceFramework's `library.json` pins the tested library graph. It cannot
select a PlatformIO platform, Arduino core, compiler, board, or partition table
for a consuming application. Those are application build inputs and must be
chosen and tested together.

## Maintained pins

| Target | Pin in maintained test projects | What it selects | Why it is pinned |
| --- | --- | --- | --- |
| ESP8266 | [`framework-arduinoespressif8266` `521ae60`](https://github.com/esp8266/Arduino/commit/521ae60a89e64bb0d1eb7a0b7addf620ced5cad3) | ESP8266 Arduino framework snapshot | Fixes Postmortem's `dangerous relocation: j: cannot encode` failure for large jump offsets. |
| ESP32 | [`pioarduino/platform-espressif32` `55.03.311`](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.311) | Arduino-ESP32 3.3.11, ESP-IDF 5.5.5, matching Xtensa 14.2 toolchain | Maintained ESP32 baseline. |

The ESP8266 linker-workaround pin and the ESP32 platform pin are independent.
Do not change one while resolving an issue with the other, and do not replace
either with a version range without compiling the affected targets.

## ESP32 Core 3 configuration

Core 3 Wi-Fi and TCP builds need the complete configuration shape below. The
tracked `platformio.ini` files are the executable source of truth.

```ini
[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
board = esp32dev
framework = arduino

build_unflags = -std=gnu++11
build_flags =
    -std=gnu++14
    -DSOC_WIFI_SUPPORTED=1
    -I${platformio.packages_dir}/framework-arduinoespressif32/libraries/Network/src

lib_ignore = ESPAsyncTCP
```

Keep the platform URL, C++ standard, Wi-Fi define, `Network/src` include path,
and ESP32 TCP selection together. A consumer using a different Core 3 release
needs the same shape of configuration and a complete build against that stack.

## Cache guidance

The maintained DeviceFramework runners use one persistent Core, package, and
download cache by default:

```text
${XDG_CACHE_HOME:-$HOME/.cache}/deviceframework-platformio/current
```

`scripts/test.sh`, `scripts/test-nonhardware.sh`, and the physical test
harnesses reuse this cache. They never clear it, uninstall packages, or force
PlatformIO to redownload dependencies for each fixture build.

This isolation avoids stale metadata in an unrelated global PlatformIO install,
including old flat `tool-esptoolpy` metadata that can shadow pioarduino's
package-form uploader. It is not a reason to add a standalone compiler override
or copy packages between framework versions.

If a specific package is suspected to be damaged, reproduce the problem first
with a disposable Core/cache. Only after confirming the exact package is bad
should it be repaired in that dedicated cache. Never clear the global cache as
a test step.

Set `DEVICEFRAMEWORK_PLATFORMIO_CORE_DIR` to move the persistent graph; package
and download directories then default beneath it. Advanced callers may set
`DEVICEFRAMEWORK_PLATFORMIO_PACKAGES_DIR` and
`DEVICEFRAMEWORK_PLATFORMIO_CACHE_DIR` explicitly. The runners use `pio` from
`PATH`, then `${HOME}/.platformio/penv/bin/pio` for non-interactive shells such
as SSH. Set `DEVICEFRAMEWORK_PIO_EXECUTABLE` only for a nonstandard install.

## Changing a target configuration

When changing a platform, framework, toolchain, or partition layout, update the
affected application or fixture configuration first, then compile every target
that shares the change. For ESP32 OTA layouts, perform the first install or
repartition over USB/serial and verify the image fits an OTA slot; see [Target
organization](TARGETS.md#esp32-ota-partitions).

Back to [documentation](README.md) · [project overview](../README.md).
