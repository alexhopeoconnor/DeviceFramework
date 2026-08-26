# DeviceFramework

DeviceFramework is the shared Arduino library for ESP8266 and ESP32 devices. It provides WiFi provisioning, persisted configuration, MQTT and Home Assistant integration, OTA, mDNS, and an optional local web interface.

## Install

Use a released Git tag. DeviceFramework's manifest installs its pinned
dependencies, including the platform-specific asynchronous TCP library:

```ini
[common]
lib_deps =
  DeviceFramework=https://github.com/alexhopeoconnor/DeviceFramework.git#v2.1.0
```

PlatformIO clones that repository and checks out the tag after `#`; it does not download a GitHub Release asset.

## Minimal sketch

```cpp
#include <Arduino.h>
#include <DeviceFramework.h>

#include "FirmwareIdentity.h"

void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
}
```

For a protected new device, select an ignored local profile containing `device_password`. It seeds the first V3 configuration record and supplies the same value to PlatformIO's `espota` uploader. That one password protects the provisioning AP, Arduino OTA, HTTP Basic authentication, and WebSerial.

After setup, the V3 record is the device's source of truth. Use `DeviceFramework::setDevicePassword("new-password")`, or the web interface's **Device Password** control, to rotate it transactionally; a valid stored password is restored on every later boot and is not overwritten by the selected profile. Update the same ignored profile before a later OTA upload, because PlatformIO needs the active password for `espota` authentication. A fresh profile-free build starts open; an existing V3 record retains its saved password.

## Local development

Released configuration uses public, pinned Git URLs. To work on sibling library checkouts without changing tracked project files, copy `platformio.local.example.ini` to an ignored file named `platformio.local.ini.<machine>` and adjust its symlink targets. `extra_configs` loads matching local overrides when present.

## Tests and releases

- `./scripts/run-tests.sh compile --platform esp8266` compiles a real clean consumer without a board (and also checks the web-disabled build).
- `./scripts/run-tests.sh hardware --platform esp8266 --port /dev/ttyUSB0 --env-file test/.env` runs the optional connected-device direct-LAN integration suite. Copy `test/.env.example`; it is ignored and no credentials are cached.
- `./scripts/run-tests.sh hardware --platform esp8266 --profile-fixture --port /dev/ttyUSB0 --env-file test/.env` additionally verifies profile password restoration and protected HTTP status, root-page, and 404 routes from the development host.
- `./scripts/prepare-release.sh v2.1.0 --tag` validates the manifest and creates an annotated tag. Push the branch and tag; GitHub Actions runs package validation and creates the GitHub Release.

See [configuration and migration](docs/CONFIGURATION.md), [compatibility](docs/COMPATIBILITY.md), [test setup](docs/TESTING.md), and the
[development and release guide](docs/DEVELOPMENT.md).
