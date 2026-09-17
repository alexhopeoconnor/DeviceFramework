# PlatformIO toolchains and package cache

DeviceFramework's `library.json` pins the tested **library** graph. It cannot
select a PlatformIO platform, Arduino core, or compiler for an application.
Those are application build inputs: each consumer must declare and test its own
`platform`, board, partition table, and compiler settings.

## Maintained test pins

| Target | Pin in the maintained test projects | What it selects | Why it is pinned |
| --- | --- | --- | --- |
| ESP8266 | `framework-arduinoespressif8266` at [`521ae60`](https://github.com/esp8266/Arduino/commit/521ae60a89e64bb0d1eb7a0b7addf620ced5cad3) | An ESP8266 Arduino framework snapshot | The upstream commit fixes Postmortem's `dangerous relocation: j: cannot encode` failure for large jump offsets. |
| ESP32 | [`pioarduino/platform-espressif32` `55.03.311`](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.311) | Arduino-ESP32 3.3.11 and ESP-IDF 5.5.5 | Current Core 3 stack used by this repository's ESP32 fixtures. |

The CI workflows pin PlatformIO Core 6.2.0. Use that version when reproducing
a resolver or package-cache failure before changing a framework pin.

The ESP32 platform URL is a pioarduino platform-package release asset. That
package, rather than DeviceFramework, declares the framework, uploader,
debugger, and compiler packages. The maintained ESP32 configurations pin
[`55.03.311`](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.311),
which selects Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5 and its matching Xtensa
14.2 toolchain. A consuming application still owns that choice: it is not
silently inherited from a DeviceFramework dependency, and a different pin must
be compiled and tested as a complete stack.

The ESP8266 workaround is documented in more detail in the shared
[ESP8266 linker-workaround note](https://github.com/alexhopeoconnor/arduino-home-assistant/blob/main/docs/ESP8266-LINKER-WORKAROUND.md).
Do not replace either pin with a version range: a framework or toolchain change
is a target-contract change and needs the normal board build checks.

## ESP32 Core 3 configuration

The Core 3 Wi-Fi and TCP layout needs the following settings in an ESP32
application that uses DeviceFramework's web stack. The maintained
`platformio.ini` files are the executable source of truth; this complete
example explains why each non-default line exists:

```ini
[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
board = esp32dev
framework = arduino

; Core 3 builds this stack as C++14 rather than PlatformIO's older default.
build_unflags = -std=gnu++11
build_flags =
    -std=gnu++14
    ; Retain the framework's conditional Wi-Fi translation units during LDF.
    -DSOC_WIFI_SUPPORTED=1
    ; Core 3 moved Network out of WiFi; make its public headers discoverable.
    -I${platformio.packages_dir}/framework-arduinoespressif32/libraries/Network/src

; ESPAsyncTCP is ESP8266-only. AsyncTCP is selected by DeviceFramework on ESP32.
lib_ignore = ESPAsyncTCP
```

Keep the platform URL, C++ standard, Wi-Fi define, `Network/src` include path,
and TCP selection together. A consumer that chooses another Core 3 pioarduino
release normally needs the same shape of configuration, but must validate that
release's resolved package graph itself.

## Inspecting and repairing the shared package store

PlatformIO installs development-platform dependencies in its global
`packages_dir`; framework, compiler, and uploader packages are therefore shared
by otherwise unrelated projects. The directory's old `package.json` metadata
does not prove what the current project requested. Inspect the resolved graph
from the consuming project instead:

```bash
pio pkg list -e esp32dev
```

For example, a project pinned to pioarduino `55.03.311` should report
Arduino-ESP32 3.3.11 and `toolchain-xtensa-esp-elf` 14.2. A report that looks
right does not prove that the global PlatformIO cache is import-safe. One
observed failure was a legacy flat `esptool.py` left in the global package
cache shadowing the package-form `esptool` required by the current pioarduino
stack. That is a cache/Python-import collision, not evidence that the project
should override its compiler or framework packages.

First reproduce with an isolated, disposable PlatformIO Core directory. This
matters because the conflicting editable-import metadata lives alongside the
Core-managed Python environment, not just in the framework package directory:

```bash
df_pio_core="$(mktemp -d /tmp/deviceframework-pio-XXXXXX)"
PLATFORMIO_CORE_DIR="$df_pio_core" \
PLATFORMIO_PACKAGES_DIR="$df_pio_core/packages" \
PLATFORMIO_CACHE_DIR="$df_pio_core/cache" \
  pio run -e esp32dev
rm -rf -- "$df_pio_core"
```

If the isolated run succeeds, retain the selected platform pin and repair the
developer's shared PlatformIO installation deliberately (or keep a dedicated
Core/cache for this project). Do **not** delete a global toolchain just because
a different stack was previously installed, and do not add a
`platform_packages` toolchain override to mask the problem: the selected
pioarduino platform owns the compatible uploader and toolchain versions.
Never commit the disposable Core/cache directory.

The maintained DeviceFramework runners already use that safer model. Their
PlatformIO calls use one dedicated Core, package directory, and cache by
default:

```text
${XDG_CACHE_HOME:-$HOME/.cache}/deviceframework-platformio/current
```

This applies to `scripts/test.sh`, `scripts/test-nonhardware.sh`, the portal
runner, the ArduinoOTA runner, and release package validation. It keeps the
ESP8266 workaround snapshot and the ESP32 3.3.11 package graph together while
keeping unrelated legacy projects out. To place the complete graph elsewhere,
set `DEVICEFRAMEWORK_PLATFORMIO_CORE_DIR`; the package and cache directories
then default beneath it. Advanced callers may set
`DEVICEFRAMEWORK_PLATFORMIO_PACKAGES_DIR` and
`DEVICEFRAMEWORK_PLATFORMIO_CACHE_DIR` explicitly as well. For example:

```bash
df_pio_core="$(mktemp -d /tmp/deviceframework-pio-XXXXXX)"
DEVICEFRAMEWORK_PLATFORMIO_CORE_DIR="$df_pio_core" \
  ./scripts/test.sh compile --platform esp32
rm -rf -- "$df_pio_core"
```

The runner never removes a persistent cache automatically. The explicit
temporary-directory command above is the only appropriate cleanup pattern for
a diagnostic cache.

## Changing a target contract

When changing a platform, framework, toolchain, or partition layout, update the
affected application or fixture configuration first, then run `pio pkg list`
and compile every target that shares the change. For ESP32 OTA layouts, perform
the first install or repartition over USB/serial and verify the image fits an
OTA slot; see [Target organization](TARGETS.md#esp32-ota-partitions).

Back to [documentation](README.md) · [project overview](../README.md).
