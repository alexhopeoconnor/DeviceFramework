# Getting started

DeviceFramework is intended to be the one direct framework dependency of an ESP8266 or ESP32 device. Start with the buildable [Portal First example](../examples/01-portal-first/) if you prefer a complete project before reading the individual setup steps.

## 1. Add the dependency

```ini
[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
lib_ldf_mode = deep+
lib_deps =
    DeviceFramework=https://github.com/alexhopeoconnor/DeviceFramework.git#v2.6.0
```

Use `#vMAJOR.MINOR.PATCH` for released firmware. PlatformIO checks out that Git tag; it does not download a GitHub Release asset.


## 2. Give the firmware an identity

Create `include/FirmwareIdentity.h`:

```cpp
#pragma once

#include <DeviceFramework.h>

namespace FirmwareIdentity {
static const char APPLICATION_ID[] = "example-device";
static const char FIRMWARE_VERSION[] = "1.0.0";
static const uint16_t CONFIGURATION_SCHEMA = 1;

inline bool configure() {
    return DeviceFramework::configureApplication(
        APPLICATION_ID, FIRMWARE_VERSION, CONFIGURATION_SCHEMA
    );
}
}
```

`APPLICATION_ID` isolates stored data between firmware families. `FIRMWARE_VERSION` describes the built application; it is independent of the DeviceFramework release tag. Increase `CONFIGURATION_SCHEMA` only when a saved value needs a semantic migration.

## 3. Register defaults, then start the framework

```cpp
void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup([]() {
        auto& registry = DeviceFrameworkParameters::getRegistry();
        registry.setDefaultValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME, "Example device");
    });
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
}
```

Register custom framework parameters and construct long-lived Home Assistant entities before calling `DeviceFramework::setup()`. ArduinoHA registers those entities automatically; do not call `DeviceFramework::getHAMqtt().addDeviceType()` in a DeviceFramework sketch. Keep `DeviceFramework::registerDeviceCommandHandler()` or `registerSharedCommandHandler()` for custom non-discovery command topics.

ArduinoHA's [entity lifecycle](https://github.com/alexhopeoconnor/arduino-home-assistant/blob/main/docs/device-and-discovery.md#discovery) explains the underlying registration and capacity rules.

Next: [configuration and profiles](CONFIGURATION.md), [web UI branding](WEB_UI.md), or the [guided examples](../examples/README.md).

Back to [documentation](README.md) · [project overview](../README.md).
