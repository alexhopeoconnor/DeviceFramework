# DeviceFramework

DeviceFramework is the ESP8266/ESP32 foundation for a complete connected device: interactive Wi-Fi provisioning, persistent configuration, MQTT and Home Assistant discovery, OTA updates, mDNS, and an optional local web interface. A firmware declares its identity and device-specific behaviour; the framework handles the shared lifecycle.

## Build a first device

```cpp
#include <DeviceFramework.h>
#include "FirmwareIdentity.h"

void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup([] {
        auto& parameters = DeviceFramework::getParameterRegistry();
        parameters.setDefaultValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME, "Example device");
    });
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
}
```

Build [Portal First](examples/01-portal-first/) for the complete project. On a clean board it opens the WiFiManager portal; after setup it exposes the existing device web interface and services.

## What it provides

- **One device lifecycle:** configuration, provisioning, Wi-Fi, mDNS, OTA, MQTT, Home Assistant, and the existing web UI work together without repeated sketch boilerplate.
- **Persistent configuration:** application-bound records keep normal parameter changes and upgrades from requiring an erase cycle.
- **Primary and fallback Wi-Fi:** a configured device can remember two networks; portal changes are verified before they persist.
- **One optional device password:** the portal, OTA, authenticated web interface, and WebSerial use one active stored value.
- **Private deployment profiles:** an ignored local JSON profile can seed a new device without putting credentials in source control.
- **Product presentation:** `DeviceFrameworkUIConfig` brands the existing web UI and provisioning portal with one source-level configuration.

## Choose an example

| Example | Start here when you want to… |
| --- | --- |
| [Portal First](examples/01-portal-first/) | make the smallest device and provision it interactively |
| [Home Assistant Telemetry](examples/02-home-assistant-telemetry/) | add a parameter plus a changing discovered telemetry entity |
| [Branded Device](examples/03-branded-device/) | give the existing admin UI and provisioning portal a product identity |
| [Managed Configuration](examples/04-managed-configuration/) | understand a safe profile template, first-boot seed, and profile-free operation |

## Install

```ini
lib_deps =
    DeviceFramework=https://github.com/alexhopeoconnor/DeviceFramework.git#v2.5.1
```

PlatformIO clones the repository and checks out the release tag after `#`. The package resolves the compatible WiFiManager, DFTE, ArduinoHA, web, and target-specific dependencies.

See [getting started](docs/GETTING_STARTED.md), [configuration](docs/CONFIGURATION.md), [web UI branding](docs/WEB_UI.md), [examples](examples/README.md), and the [documentation index](docs/README.md).
