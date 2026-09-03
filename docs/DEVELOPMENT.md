# Development and releases

This guide is for people working on DeviceFramework itself. Application firmware
should depend on a released DeviceFramework tag; it should not declare
WiFiManager, DFTE, or ArduinoHA separately. The package's
[`library.json`](../library.json) is the single source of the tested dependency
set, including each platform-specific asynchronous TCP package. The clean
consumer and framework test project both use LDF `deep+` so PlatformIO follows
framework-library headers as well as source files.


## Build without hardware

PlatformIO and Python 3 are required. The compile checks build a minimal clean
consumer which declares only DeviceFramework; they do not upload or execute
firmware, so no board or credentials are needed:

```bash
./scripts/test.sh compile --platform esp8266
./scripts/test.sh compile --platform esp8266 --profile-fixture
./scripts/test.sh compile --platform esp32
./scripts/test.sh compile --platform esp32 --profile-fixture
```

The runner refreshes its generated PlatformIO package cache before each check, so
CI and local runs compile the current source rather than a stale `.pio` copy.
CI runs these normal and profile-fixture checks for every push and pull request, and the tag workflow repeats them before it creates a GitHub Release.

The normal ESP8266 command also checks the web-interface-free configuration.

## ESP32 framework pin

The maintained ESP32 environments use the PlatformIO-compatible
`pioarduino/platform-espressif32` 51.03.05 package. It packages Espressif's
official Arduino-ESP32 3.0.5 framework and its matching ESP-IDF 5.1 libraries;
it is not a GitHub Release asset used by application firmware. The URL in each
`platformio.ini` is the pinned PlatformIO platform package that downloads those
framework inputs.

This pin is deliberate. PlatformIO official `espressif32` packages available
to this stack still provide Arduino-ESP32 2.0.17. On the affected board that
version starts an asynchronous WiFi scan and reports `WIFI_SCAN_FAILED` after
exactly six seconds. The current framework initializes the native scan
configuration correctly. The AP+STA probe and portal-like scan complete in
about four seconds on the connected ESP32.

Core 3 separates the `Network` library from `WiFi`. Projects using LDF `deep+`
therefore need `-DSOC_WIFI_SUPPORTED=1` and the documented `Network/src`
include path so PlatformIO finds conditional WiFi sources and the new AsyncTCP
header. ESP32 environments also ignore `ESPAsyncTCP`, which is the ESP8266-only
transport; `AsyncTCP` is selected instead. The maintained PlatformIO files
already carry these settings—copy them when creating another ESP32 consuming
sketch.

## Run the connected-device suite

The optional hardware suite exercises WiFi, MQTT, V4 storage, password
persistence, web-interface restart, and direct HTTP requests from the
development host to a connected board. It needs
curl and Avahi for automatic mDNS discovery (or an explicit device address), a reachable MQTT broker, a WiFi network,
and an unused development device. Copy the template;
the real file is ignored and must never be committed:

```bash
cp test/.env.example test/.env
./scripts/test.sh hardware --platform esp8266 --port /dev/ttyUSB0
```

Use `--env-file path/to/file` when the credentials live outside the repository.
The runner creates its generated test header only for the run and removes it on
exit. It uploads the Unity image with a bootstrap profile, then owns the serial
port while it sends the same RTS-only hard reset esptool normally uses after
upload and captures the result. It requires a non-empty zero-failure Unity
result before proceeding.

For a profile fixture, it then flashes the minimal consuming sketch with a
separate reconcile-profile identity. That makes the WiFi seed apply after the
Unity run has left a valid V4 record, exercising the intended non-erased-device
path. The runner waits for its unique mDNS name before it verifies authenticated
status, pages, CSS/JavaScript/logo assets, and password persistence through a
reboot. Set `DEVICEFRAMEWORK_TEST_DEVICE_HOST` in the ignored env file when
Avahi or mDNS is unavailable.

## Work against sibling checkouts

The consumer compile fixture is intentionally a separate PlatformIO project. If
you are working before the dependent library tags exist, copy its local template
as well so it resolves the sibling checkouts rather than remote release tags:

```bash
cp test/compile-project/platformio.local.example.ini test/compile-project/platformio.local.ini.<machine>
```

Released builds use public Git tags. For coordinated local library development, copy
[`platformio.local.example.ini`](../platformio.local.example.ini) to a file such
as `platformio.local.ini.alex`, update its `lib_extra_dirs` path, and leave that
file untracked. It discovers sibling source trees while the explicit `lib_deps`
list supplies only their third-party dependencies. `platformio.ini` loads matching
`platformio.local.ini.*` files when present, so no tracked configuration or
application dependency needs to change.

## Publish a release

1. Release dependencies before their consumers: DFTE and ArduinoHA, then WiFiManager (which pins DFTE), then DeviceFramework (which pins all three).
2. Start each release with the one deterministic metadata update:

   ```bash
   ./scripts/bump-version.sh vMAJOR.MINOR.PATCH
   ```

3. Replace the generated changelog TODO with the release summary. Update the compatibility table only when a pinned dependency or target contract changes; update guides when behaviour changes.
4. Run the four compile checks above; run the hardware suite when its covered
   behavior changed.
5. Commit the release preparation, then validate and create the annotated tag:

   ```bash
   ./scripts/prepare-release.sh vMAJOR.MINOR.PATCH --tag
   ```


6. Push the branch and tag. The tag workflow repeats the compile checks, validates release metadata and the PlatformIO package, and creates the GitHub Release from that tag.
The `#vMAJOR.MINOR.PATCH` portion of a PlatformIO Git dependency is a Git ref:
PlatformIO clones the repository and checks out that tag. It does not download
a GitHub Release asset. Tags therefore provide reproducible dependency inputs;
an exact commit SHA is an even stricter pin for short-lived diagnostics.

Back to [documentation](README.md) · [project overview](../README.md).
