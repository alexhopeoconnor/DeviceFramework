# Home Assistant and MQTT

DeviceFramework owns the Wi-Fi-to-MQTT connection lifecycle and configures one
ArduinoHA `HADevice` and `HAMqtt` client. A sketch adds two kinds of behaviour:

- **Configuration parameters** for durable editable settings, which the
  framework turns into Home Assistant number, switch, select, or text entities.
- **Native ArduinoHA entities** for telemetry and device behaviour such as
  sensors, lights, switches, covers, and binary sensors.

The framework does not require a connected broker to run local device logic.
MQTT begins only after usable Wi-Fi and broker resolution are available, and it
reconnects as the network changes.

![Home Assistant native device page showing a DeviceFramework fixture with
discovered telemetry, controls, and editable parameters.](assets/readme/home-assistant-device-page.png)

Home Assistant renders this page from MQTT discovery and state messages. The
framework supplies that integration surface; it does not render or style Home
Assistant's own interface.

## Pick the right integration surface

| Requirement | Use | Why |
| --- | --- | --- |
| Editable interval, threshold, mode, or calibration | A framework parameter with `SOURCE_HOME_ASSISTANT` | One validated, stored value across portal and HA. |
| Sensor measurement, health, availability, button, light, or cover | A long-lived native ArduinoHA entity | Models live device state and standard HA semantics. |
| Product-specific command not represented by an HA entity | `registerDeviceCommandHandler` or `registerSharedCommandHandler` | Provides a deliberately scoped custom MQTT command subscription. |
| Broker/configuration access | `getHAMqtt()`, `getHADevice()`, core parameters | The framework retains connection and discovery ownership. |

Do not manually call ArduinoHA device-registration methods that DeviceFramework
already performs. It registers the native entities that exist at framework setup
and the HA-backed parameters it creates from the registry.

## Native entities: construct early, update later

Create entities with lifetime for the whole program—normally namespace/static
scope—and configure metadata before `DeviceFramework::setup()`:

```cpp
#include <ArduinoHA.h>
#include <DeviceFramework.h>

namespace {
HASensorNumber temperature("temperature", HASensorNumber::PrecisionP1);
HABinarySensor sensorHealthy("sensor_healthy");

void configureEntities() {
    temperature.setName("Temperature");
    temperature.setUnitOfMeasurement("°C");
    temperature.setDeviceClass("temperature");
    sensorHealthy.setName("Temperature sensor healthy");
    sensorHealthy.setDeviceClass("connectivity");
}
}

void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup(registerParameters);
    configureEntities();
    DeviceFramework::setup();
}

void publishReading(float celsius, bool valid) {
    sensorHealthy.setState(valid);
    if (valid) temperature.setValue(celsius);
}
```

The entities need not publish while disconnected; ArduinoHA/DeviceFramework
handles discovery and later connection/resynchronisation. Never create them in
`loop()` or from a parameter callback: repeated construction consumes capacity
and produces unstable discovery identity.

## Framework-managed HA configuration

For a setting that Home Assistant should edit, declare a parameter rather than
hand-writing parallel MQTT parsing. For example, a selectable motion-sensor
mode:

```cpp
DeviceFrameworkParameterMetadata mode;
mode.id = "motionmode";
mode.label = "Motion sensitivity";
mode.defaultValue = "normal";
mode.maxLength = 8;
mode.order = 80;
mode.valueType = DeviceFrameworkParameterValueType::Enum;
mode.allowedValues = "quiet;normal;high";
mode.sources = SOURCE_WIFI_MANAGER | SOURCE_HOME_ASSISTANT;
mode.htmlAttributes.inputType = "select";
mode.htmlAttributes.options = "quiet;normal;high";
mode.haDeviceType = HAConfigDeviceType::SELECT;
mode.haConstraints.options = "quiet;normal;high";
DeviceFramework::getParameterRegistry().registerParameter(mode);
```

The framework validates incoming portal and HA values, updates the registry,
persists framework-originated edits, keeps the other supported surface in sync,
and performs a paced full parameter re-sync after MQTT returns. Your change
callback should only request a non-blocking hardware update; see
[Parameters](PARAMETERS.md).

## Connection-aware application work

Most entities can be updated whenever your application has a new value. For
one-time connected work—such as a custom retained diagnostic or an immediate
application state publish—register the callback before the first framework
loop:

```cpp
void publishApplicationState() {
    outputEnabledSensor.setState(appliedOutput);
}

void setup() {
    // Identity, parameters, native entity metadata, and setup omitted.
    DeviceFramework::setup();
    DeviceFrameworkMQTT::onConnected(publishApplicationState);
}
```

The callback may run again after reconnect. Make it idempotent and quick.
`DeviceFrameworkMQTT::isConnected()` is suitable for optional work, not a reason
to stall application control or provisioning.

## Custom MQTT commands

Use custom commands only for behaviour that cannot be expressed as a normal
ArduinoHA entity or parameter. Declare a parser that validates the bounded
payload and transitions an application state machine:

```cpp
void onEffectCommand(const uint8_t* payload, const uint16_t length) {
    if (length == 4 && memcmp(payload, "wave", 4) == 0) {
        requestedEffect = Effect::Wave;  // Loop applies it without blocking.
    }
}

void setup() {
    // ... DeviceFramework::setup();
    DeviceFramework::registerDeviceCommandHandler(&statusLight,
                                                   "effect", onEffectCommand);
    DeviceFramework::addMQTTRestartCommand();
}
```

`registerDeviceCommandHandler` namespaces a topic for a particular ArduinoHA
entity; `registerSharedCommandHandler` creates a device-wide topic. The
framework subscribes registered handlers when MQTT connects. Register before
the first `DeviceFramework::loop()` so a fast connection cannot pass the
subscription point first. Do not use raw topic strings to duplicate ordinary HA
command topics.

## Identity, discovery, and configuration changes

DeviceFramework sets the HA device identity from the hardware MAC, current
device name, and configured firmware version. A device-name edit is durable but
does not rename already-initialised mDNS/OTA transports in place; restart to
apply network identity changes. Firmware/application identity and configuration
schema are described in [Getting started](GETTING_STARTED.md) and
[Configuration and profiles](CONFIGURATION.md).

The [Home Assistant Telemetry](../examples/02-home-assistant-telemetry/)
example is the small runnable starting point; the real-world scenarios explain
more involved patterns.

Next: [API reference](API_REFERENCE.md) · [Presence-aware lighting](scenarios/presence-aware-lighting.md) · [Operations](OPERATIONS.md).
