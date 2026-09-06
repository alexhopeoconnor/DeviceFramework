# Device lifecycle

DeviceFramework starts the shared device services in a fixed order. A sketch supplies its identity, setup-time declarations, hardware policy, and the non-blocking work it performs after each framework loop.

## The contract

1. Make any hazardous output safe before networking or configuration can run.
2. Configure the firmware identity and source-owned presentation policy.
3. Create native ArduinoHA entities that live for the whole firmware lifetime.
4. Call `beforeSetup()` to register defaults and custom parameters.
5. Call `setup()` exactly once. It loads saved configuration, may apply an eligible local profile, and prepares Wi-Fi, OTA, MQTT/Home Assistant, and optional web services.
6. Apply the now-loaded values to local hardware. Storage loading intentionally does not call a sketch's parameter-change callback.
7. Call `DeviceFramework::loop()` on every pass through `loop()`. It owns provisioning, connection transitions, mDNS, OTA, MQTT, Home Assistant, and optional web work.

## A complete baseline

```cpp
#include <DeviceFramework.h>
#include "FirmwareIdentity.h"

namespace {
void makeHardwareSafe() {
    // Configure every product-specific output pin and drive its electrically
    // safe level here. Keep board pin assignments in application code.
}

void applyLoadedState() {
    // Read parameters and apply them to hardware after DeviceFramework::setup().
}
}  // namespace

void setup() {
    makeHardwareSafe();
    FirmwareIdentity::configure();

    DeviceFramework::beforeSetup([] {
        auto& parameters = DeviceFramework::getParameterRegistry();
        parameters.setDefaultValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME,
                                   "Workshop controller");
        // Register custom parameters here.
    });

    DeviceFramework::setup();
    applyLoadedState();
}

void loop() {
    DeviceFramework::loop();
    // Run bounded application work here; do not wait for network state.
}
```

The example uses an output pin only to demonstrate order. Do not attach it to
mains, a battery charger, or another consequential load without an
application-specific electrical safety design. The runnable
[Protected Output](../examples/05-protected-output/) example uses a configured
demonstration indicator for that reason.

## What happens at each phase

| Phase | Framework work | Sketch responsibility |
| --- | --- | --- |
| Before `beforeSetup()` | Nothing has initialized storage or network services. | Set output pins to their safe electrical state; configure `DeviceFrameworkUIConfig` and web limits if used. |
| `beforeSetup(callback)` | Initializes storage and core parameters, runs `callback`, then handles reset tracking. | Register every custom parameter and set defaults. Do not start Wi-Fi, MQTT, or long work. |
| `setup()` | Loads the saved record, applies an eligible profile, then configures Wi-Fi, OTA, HA/MQTT, and optional web service. | Keep native HA entities alive and configure their metadata before this call. Then apply loaded state to hardware. |
| `loop()` before usable Wi-Fi | Services the WiFiManager portal and reset/web restart plumbing. | Continue only bounded local work. Portal mode is a normal state. |
| `loop()` after stable Wi-Fi | Starts/maintains mDNS, OTA, MQTT, HA discovery/state, and optional web work. | Continue sensor sampling, state machines, and output enforcement without blocking. |

`setup()` is intentionally not a guarantee that MQTT is connected. A new board
may be in the provisioning portal, a configured board may be reconnecting, and
DNS/broker availability may arrive later. Use `DeviceFrameworkMQTT::onConnected`
or `DeviceFrameworkMQTT::isConnected()` only for work that specifically needs a
broker; never defer local safety or control behaviour until MQTT is up.

## Ordering rules that avoid subtle bugs

- Call `FirmwareIdentity::configure()` before `beforeSetup()`/`setup()`. It
  identifies the storage family and version reported to Home Assistant.
- Call `DeviceFramework::setUIConfig()` and
  `DeviceFrameworkWeb::setResourceLimits()` before framework setup. Both are
  source-owned runtime policy and lock once services begin.
- Construct native ArduinoHA entities at static/long-lived scope and configure
  their metadata before `DeviceFramework::setup()`. Framework-managed
  parameters are registered in the `beforeSetup` callback and become HA
  entities during setup.
- Register custom MQTT command handlers after `setup()` but before the first
  loop if the handler requires the framework MQTT object. They are subscribed
  when the broker connects.
- Put normal hardware application in `applyLoadedState()` and in non-blocking
  loop code. A parameter callback represents a change, not an initial-value
  notification.

## Portal mode is not an error branch

If no usable saved Wi-Fi profile exists, `DeviceFramework::loop()` runs the
WiFiManager provisioning portal. The framework does **not** start the normal
network-service flow until Wi-Fi has a usable address. Your firmware should
still keep its local safety policy, sensor state machine, and physical controls
alive while provisioning is open. See [Portal First](../examples/01-portal-first/)
for the smallest executable path and [Operations](OPERATIONS.md) for recovery.

## Hardware state after reboot

Persisted configuration can describe a desired policy, but it cannot make an
output electrically safe during reset. Drive safe GPIO levels first, then read
the loaded configuration and calculate the actual permitted output. For a
latched but restart-resilient *intent*, `DeviceFrameworkRtcBlob` is available;
its ESP8266 single-slot constraint means it is unsuitable as a general state
database. The [protected-output scenario](scenarios/protected-output-controller.md)
shows the distinction between requested and applied state.

Next: [Parameters](PARAMETERS.md) · [Home Assistant and MQTT](HOME_ASSISTANT_MQTT.md) · [API reference](API_REFERENCE.md).
