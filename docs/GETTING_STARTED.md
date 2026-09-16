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
    DeviceFramework=https://github.com/alexhopeoconnor/DeviceFramework.git#v2.8.3
```

Use `#vMAJOR.MINOR.PATCH` for released firmware. PlatformIO checks out that Git tag; it does not download a GitHub Release asset.


## 2. Give the firmware an identity

Create `include/FirmwareIdentity.h`:

```cpp
#pragma once

#include <DeviceFramework.h>

namespace FirmwareIdentity {
static const char APPLICATION_ID[] = "example-device";  // Stable across ordinary releases.
static const char FIRMWARE_VERSION[] = "1.0.0";         // Reported by the running device.
static const uint16_t CONFIGURATION_SCHEMA = 1;           // Changes only with a semantic migration.

inline bool configure() {
    // Must run before framework setup reads persisted configuration.
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

    // Register defaults and entities before saved configuration is loaded.
    DeviceFramework::beforeSetup([]() {
        auto& registry = DeviceFramework::getParameterRegistry();
        registry.setDefaultValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME, "Example device");
    });
    DeviceFramework::setup();  // Starts Wi-Fi, MQTT, OTA, and optional web services.
}

void loop() {
    DeviceFramework::loop();  // Services the framework instead of duplicating subsystem loops.
}
```

Register custom framework parameters in the `beforeSetup` callback. Construct
long-lived native ArduinoHA entities and configure their metadata before
`DeviceFramework::setup()`; DeviceFramework registers them automatically. Do
not call `DeviceFramework::getHAMqtt().addDeviceType()` in a DeviceFramework
sketch.

After `setup()`, apply the values that were loaded from storage to your hardware.
Register `DeviceFramework::registerDeviceCommandHandler()` or
`registerSharedCommandHandler()` before the first loop only for custom
non-discovery command topics. Parameter changes and MQTT handlers should queue
or flag hardware work rather than block.

ArduinoHA's [entity lifecycle](https://github.com/alexhopeoconnor/arduino-home-assistant/blob/main/docs/device-and-discovery.md#discovery) explains the underlying registration and capacity rules.

Next: [Lifecycle](LIFECYCLE.md), [Parameters](PARAMETERS.md), [Home Assistant and MQTT](HOME_ASSISTANT_MQTT.md), [configuration and profiles](CONFIGURATION.md), or the [guided examples](../examples/README.md).

Back to [documentation](README.md) · [project overview](../README.md).
