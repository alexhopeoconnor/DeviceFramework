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

Build [Portal First](examples/01-portal-first/) for the complete project. On a clean board it starts the WiFiManager provisioning portal. After you enter valid Wi-Fi details, the device saves them and restarts; later boots reconnect and run mDNS, OTA, MQTT, and—when built with `ENABLE_WEB_INTERFACE`—the existing local web interface. This skeleton provides the shared lifecycle only: add your own parameters, sensors, controls, and Home Assistant entities for device-specific behaviour.

## What it provides

- **One device lifecycle:** configuration, provisioning, Wi-Fi, mDNS, OTA, MQTT, Home Assistant, and the existing web UI work together without repeated sketch boilerplate.
- **Persistent configuration:** application-bound records keep normal parameter changes and upgrades from requiring an erase cycle.
- **Primary and fallback Wi-Fi:** a configured device can remember two networks; portal changes are verified before they persist.
- **One optional device password:** the portal, OTA, authenticated web interface, and WebSerial use one active stored value.
- **Private deployment profiles:** an ignored local JSON profile can seed a new device without putting credentials in source control.
- **Product presentation:** `DeviceFrameworkUIConfig` brands the existing web UI and provisioning portal with one source-level configuration.
- **Measured web resilience:** platform-aware response and WebSerial limits keep a busy ESP8266 responsive while remaining configurable by a sketch.

## Built with maintained libraries

DeviceFramework composes tested releases of:

- [WiFiManager](https://github.com/alexhopeoconnor/WiFiManager) — self-hosted Wi-Fi provisioning.
- [DFTE](https://github.com/alexhopeoconnor/DFTE) — bounded streaming templates for embedded web responses.
- [Arduino Home Assistant integration](https://github.com/alexhopeoconnor/arduino-home-assistant) — MQTT discovery and entities.

A firmware normally declares **DeviceFramework only**. Its package manifest
resolves compatible library releases; see [Compatibility](docs/COMPATIBILITY.md)
for the exact mapping.

## Choose an example

| Example | Start here when you want to… |
| --- | --- |
| [Portal First](examples/01-portal-first/) | make the smallest device and provision it interactively |
| [Home Assistant Telemetry](examples/02-home-assistant-telemetry/) | add a parameter plus a changing discovered telemetry entity |
| [Branded Device](examples/03-branded-device/) | give the existing admin UI and provisioning portal a product identity |
| [Managed Configuration](examples/04-managed-configuration/) | understand a safe profile template, first-boot seed, and profile-free operation |
| [Protected Output](examples/05-protected-output/) | model safe boot, requested versus applied state, and a bounded output policy using a configured demo indicator |

## Install

```ini
lib_deps =
    DeviceFramework=https://github.com/alexhopeoconnor/DeviceFramework.git#v2.8.3
```

PlatformIO clones the repository and checks out the release tag after `#`. The
package resolves the compatible libraries, web server, and target-specific
dependencies.

See the [documentation index](docs/README.md) for the complete map, including
[getting started](docs/GETTING_STARTED.md), [configuration](docs/CONFIGURATION.md),
[web UI branding](docs/WEB_UI.md), [web resource limits](docs/WEB_RESOURCES.md),
and [examples](examples/README.md).

## Documentation by task

Start with the small sketch above, then follow the guide that matches what you
want to add.

| When you need to… | Read |
| --- | --- |
| Understand exact `setup()`/`loop()` ordering, portal mode, and hardware boot policy | [Lifecycle](docs/LIFECYCLE.md) |
| Add durable editable settings with portal and Home Assistant presentation | [Parameters](docs/PARAMETERS.md) |
| Add native ArduinoHA entities or a custom MQTT command | [Home Assistant and MQTT](docs/HOME_ASSISTANT_MQTT.md) |
| Look up supported sketch-facing APIs and advanced constraints | [API reference](docs/API_REFERENCE.md) |
| Deploy, reset, rotate credentials, or diagnose field behaviour | [Operations](docs/OPERATIONS.md) and [Troubleshooting](docs/TROUBLESHOOTING.md) |
| Build a sensor, protected controller, presence light, or product family | [Real-world scenarios](docs/SCENARIOS.md) |
