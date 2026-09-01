# DeviceFramework

DeviceFramework is the ESP8266/ESP32 foundation for devices that need WiFi provisioning, persistent configuration, MQTT and Home Assistant integration, OTA, mDNS, and an optional local web interface.

## Why use it

- **One device lifecycle:** initialise a device once, then let the framework manage provisioning, WiFi, MQTT, OTA, and the web UI.
- **Configuration that survives real upgrades:** DFC4 records are application-bound and schema-aware, so routine parameter changes do not require erasing devices.
- **Network resilience without duplicate setup:** a profile can define a primary WiFi network and one fallback; successful portal changes are verified before they persist.
- **One optional device password:** provisioning, OTA, HTTP Basic authentication, and WebSerial share one persisted password.
- **Repeatable deployment:** an ignored local profile can seed a new board without placing credentials in source control.
- **One branded product experience:** configure the existing web admin UI and WiFi provisioning portal once with `DeviceFrameworkUIConfig`.

## Try it

Each firmware declares its own identity, then uses the same compact setup/loop skeleton:

```cpp
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

The consuming firmware registers its parameters and entities in `beforeSetup()`; see the [getting-started guide](docs/GETTING_STARTED.md) for the complete flow.


To give both existing interfaces a product name and semantic theme, set `DeviceFrameworkUIConfig` once before setup:

```cpp
const char kBrand[] PROGMEM = "Tree";
const char kProduct[] PROGMEM = "Temperature Monitor";
const char kAccent[] PROGMEM = "#347a45";

DeviceFrameworkUIConfig ui;
ui.branding.brandName = DeviceFrameworkText::progmem(kBrand);
ui.branding.productName = DeviceFrameworkText::progmem(kProduct);
ui.theme.accent = DeviceFrameworkText::progmem(kAccent);
DeviceFramework::setUIConfig(ui);  // web admin + provisioning portal
```

`productName` is the existing web heading and default browser title; `webTitle` is an optional browser-title override. See [Web UI and provisioning branding](docs/WEB_UI.md) for the complete mapping, themes, and static logo support.

## Install

Use a released Git tag. DeviceFramework's manifest resolves the tested WiFiManager, DFTE, ArduinoHA, async-web, and platform-specific TCP dependencies:

```ini
[common]
lib_deps =
  DeviceFramework=https://github.com/alexhopeoconnor/DeviceFramework.git#v2.2.0
```

The text after `#` is a Git ref. PlatformIO clones the repository and checks out that tag; it does not download a GitHub Release asset.

## Start here

| Goal | Guide |
| --- | --- |
| Build a first device or understand the setup lifecycle | [Getting started](docs/GETTING_STARTED.md) |
| Provision with a private profile, migrate V4 data, or rotate a password | [Configuration and profiles](docs/CONFIGURATION.md) |
| Brand the existing admin UI and provisioning portal together | [Web UI and provisioning branding](docs/WEB_UI.md) |
| See tested dependency versions and supported targets | [Compatibility](docs/COMPATIBILITY.md) |
| Run compile or connected-device checks | [Testing](docs/TESTING.md) |
| Work on the framework or prepare a release | [Development and releases](docs/DEVELOPMENT.md) |

## Local profile in one sentence

For a new protected device, select an ignored local profile containing `device_password`. It seeds the first V4 record and gives PlatformIO the same OTA password. Later password changes persist in V4 storage; update that local JSON before the next OTA upload so `espota` can authenticate.

## Development and releases

```bash
./scripts/test.sh compile --platform esp8266
./scripts/test.sh compile --platform esp32
./scripts/check-docs.sh
./scripts/prepare-release.sh vMAJOR.MINOR.PATCH --tag
```

The tag workflow repeats the board-free compile checks, validates a PlatformIO package, and creates a GitHub Release from the matching changelog section. It does not publish to the PlatformIO Registry or deploy firmware.

See the [documentation index](docs/README.md), [release history](CHANGELOG.md), and [MIT licence](LICENSE).
