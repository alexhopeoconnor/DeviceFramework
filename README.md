# DeviceFramework

DeviceFramework is the shared Arduino library for ESP8266 and ESP32 devices. It provides WiFi provisioning, persisted configuration, MQTT and Home Assistant integration, OTA, mDNS, and an optional local web interface.

## Install

Use a released Git tag in `platformio.ini`:

```ini
lib_deps =
  DeviceFramework=https://github.com/alexhopeoconnor/DeviceFramework.git#v1.0.0
```

PlatformIO clones that repository and checks out the tag after `#`; it does not download a GitHub Release asset. The `library.json` at the repository root supplies the tagged transitive dependencies.

## Minimal sketch

```cpp
#include <Arduino.h>
#include <DeviceFramework.h>

void setup() {
    // Optional. One password protects the provisioning AP, Arduino OTA,
    // HTTP Basic authentication, and WebSerial.
    // setConfigDevicePassword("a-local-device-password");
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
#ifdef ENABLE_WEB_INTERFACE
    DeviceFramework::webInterfaceLoop();
#endif
}
```

An empty password intentionally leaves those local surfaces open. A non-empty password must contain 8–31 characters so WPA2 provisioning, OTA, HTTP Basic authentication, and WebSerial remain aligned; `setConfigDevicePassword()` returns `false` if that validation fails. The legacy `setConfigAdminPassword()` API remains an alias for source compatibility.

## Local development

Released configuration uses public, pinned Git URLs. To work on sibling library checkouts without changing tracked project files, copy `platformio.local.example.ini` to an ignored file named `platformio.local.ini.<machine>` and adjust its symlink targets. `extra_configs` loads matching local overrides when present.

## Tests and releases

- `./scripts/run-tests.sh compile --platform esp8266` compiles the full hardware test firmware without a board.
- `./scripts/run-tests.sh hardware --platform esp8266 --port /dev/ttyUSB0 --env-file test/.env` runs the optional connected-device/Docker integration suite. Copy `test/.env.example`; it is ignored and no credentials are cached.
- `./scripts/prepare-release.sh v1.0.0 --tag` validates the manifest and creates an annotated tag. Push the branch and tag; GitHub Actions runs package validation and creates the GitHub Release.

See [compatibility](docs/COMPATIBILITY.md), [test setup](docs/TESTING.md), and the
[development and release guide](docs/DEVELOPMENT.md).
