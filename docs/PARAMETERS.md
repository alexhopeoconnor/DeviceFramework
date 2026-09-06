# Parameters and device configuration

The parameter registry is DeviceFramework's configuration boundary. Register a
stable ID once, give it defaults and validation/presentation metadata, then let
the framework present and persist the same value across the provisioning portal,
Home Assistant, and the application. It is well suited to operator-adjustable
settings such as an interval, threshold, mode, or calibration offset.

It is not a replacement for a device's runtime state machine. A relay request,
sensor fault, measured value, and the resulting applied output often need their
own explicit application state.

## Register parameters before setup

Use `DeviceFramework::getParameterRegistry()` inside the `beforeSetup`
callback. Register every custom parameter before `DeviceFramework::setup()`;
the framework creates portal fields and HA configuration entities from that
schema during setup.

```cpp
namespace {
constexpr char kSampleInterval[] = "sampleinterval";
}

void registerParameters() {
    auto& parameters = DeviceFramework::getParameterRegistry();

    DeviceFrameworkParameterMetadata interval;
    interval.id = kSampleInterval;                 // Permanent storage/API key.
    interval.label = "Sample interval";           // UI label; may change later.
    interval.defaultValue = "10";
    interval.maxLength = 5;
    interval.order = 50;
    interval.valueType = DeviceFrameworkParameterValueType::UnsignedInteger;
    interval.hasNumericRange = true;
    interval.minValue = 1;
    interval.maxValue = 60;
    interval.sources = SOURCE_WIFI_MANAGER | SOURCE_HOME_ASSISTANT;
    interval.htmlAttributes.inputType = "number";
    interval.htmlAttributes.inputmode = "numeric";
    interval.haDeviceType = HAConfigDeviceType::NUMBER;
    interval.haConstraints.minValue = 1;
    interval.haConstraints.maxValue = 60;
    interval.haConstraints.step = 1;
    interval.haConstraints.precision = 0;
    interval.haUnitOfMeasurement = "s";
    parameters.registerParameter(interval);
}

void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup(registerParameters);
    DeviceFramework::setup();
}
```

`id` is the durable contract. It is used by storage, an optional local profile,
and generated integration identifiers. Treat it as an API key: use short,
lower-case, alphanumeric IDs and do not rename one merely to improve a label.
A label, icon, unit, entity presentation, or default can evolve without a
configuration-schema migration. A semantic change to a saved value requires a
migration; see [Configuration and profiles](CONFIGURATION.md).

## Schema checklist

| Field | Use it for | Notes |
| --- | --- | --- |
| `id` | Stable stored identity | Unique and immutable once devices are deployed. |
| `label` | Portal and Home Assistant presentation | Human-friendly and safe to revise. |
| `defaultValue` | First-boot/missing-value policy | Is overridden by saved configuration. |
| `maxLength` | Storage/input limit | Pick a real upper bound; long text costs memory. |
| `order` | Field/entity ordering | Lower values appear first. |
| `valueType`, numeric range, `allowedValues` | Validation | Define the accepted shape before exposing a setting. |
| `sources` | Where an operator may edit it | `SOURCE_WIFI_MANAGER`, `SOURCE_HOME_ASSISTANT`, or both. |
| `htmlAttributes` | Portal input presentation | Use `number`, `select`, input mode, and options where appropriate. |
| `haDeviceType`, constraints | HA configuration entity | Use only an appropriate editable HA type: number, switch, select, or text. |

For enum/select values, use the same semicolon-separated options on both
surfaces:

```cpp
mode.valueType = DeviceFrameworkParameterValueType::Enum;
mode.allowedValues = "auto;quiet;boost";
mode.sources = SOURCE_WIFI_MANAGER | SOURCE_HOME_ASSISTANT;
mode.htmlAttributes.inputType = "select";
mode.htmlAttributes.options = "auto;quiet;boost";
mode.haDeviceType = HAConfigDeviceType::SELECT;
mode.haConstraints.options = "auto;quiet;boost";
```

## Defaults, saved values, profiles, and live updates

There is one normal runtime authority: the current saved configuration. The
flow is:

```text
registered defaults
        ↓ (on setup)
matching saved record overrides defaults
        ↓
eligible local profile may seed/reconcile named values once
        ↓
current registry values drive the application
```

An ordinary reboot does not reapply a selected profile. A profile is a build
input with explicit bootstrap/reconcile rules, not an alternative runtime
database. Full details are in [Configuration and profiles](CONFIGURATION.md).

Portal and HA-originated writes are persisted by their framework paths.
Direct application calls to `registry.setValue()` or
`DeviceFramework::setCustomParameterValue()` update in-memory state and sync
supported surfaces, but do **not** implicitly save. Call
`DeviceFramework::saveParameters()` only when the application deliberately
wants to commit such a direct change:

```cpp
auto& parameters = DeviceFramework::getParameterRegistry();
if (parameters.setValue(kSampleInterval, 15)) {
    DeviceFramework::saveParameters();
}
```

Avoid writing every sensor value through the registry. Configuration storage has
a purposefully different write pattern than telemetry.

## Apply initial and changed values safely

Set one change callback after declaring the schema. It runs only when a value
actually changes; loading persisted values at setup does not replay it. Apply
the loaded state explicitly after `DeviceFramework::setup()`, then use the
callback for later edits:

```cpp
bool reconfigureRequested = false;

void requestReconfigure(const String& id, const String&, const String&) {
    if (id == kSampleInterval) {
        reconfigureRequested = true;
    }
}

void applyLoadedState() {
    const int seconds = DeviceFramework::getParameterRegistry()
                            .getValueAsInt(kSampleInterval);
    configureSampling(constrain(seconds, 1, 60));
}

void setup() {
    // identity + beforeSetup(registerParameters) omitted
    DeviceFramework::getParameterRegistry().setChangeCallback(requestReconfigure);
    DeviceFramework::setup();
    applyLoadedState();
}

void loop() {
    DeviceFramework::loop();
    if (reconfigureRequested) {
        reconfigureRequested = false;
        applyLoadedState();
    }
    runSamplingStateMachine();
}
```

Keep the callback short. In particular, a Home Assistant command can invoke it
while MQTT dispatch is active. Set a flag, update a cheap shadow value, or queue
one step of work; do not wait for a sensor, block on I/O, repeatedly allocate,
or perform a long peripheral transaction inside it. The
[presence-aware lighting scenario](scenarios/presence-aware-lighting.md) shows
why.

## Parameters versus application state

Use a parameter when an operator selects a durable policy. Use an application
state machine when the firmware must decide what is actually safe or possible.

| Need | Recommended representation |
| --- | --- |
| “Turn off below this battery voltage” | Persisted numeric parameter. |
| “Operator requests output on” | Application request state, usually reflected by a native HA entity. |
| “Fault input is active” | Measured/runtime state, optionally published as a binary sensor. |
| “Relay is physically energised” | Applied runtime state; report this, not merely the request. |
| “Resume a small request after a watchdog reset” | Carefully scoped `DeviceFrameworkRtcBlob` data, not a parameter write on every loop. |

The [protected-output scenario](scenarios/protected-output-controller.md) uses
this separation to prevent a configuration change from becoming an unsafe
hardware command.

## Core parameters and convenience accessors

DeviceFramework registers device name, MQTT host/port/user/password, and log
level before invoking your callback. Use the named facade for common values:

```cpp
const char* name = DeviceFramework::getDeviceName();
const char* broker = DeviceFramework::getMqttServer();
uint16_t port = DeviceFramework::getMqttPort();
```

The generic registry remains the canonical extension surface. Prefer
`DeviceFramework::getParameterRegistry()` in new code. The older
`DeviceFrameworkParameters::getRegistry()` facade remains available, but mixing
both styles in one sketch obscures the lifecycle.

Next: [Home Assistant and MQTT](HOME_ASSISTANT_MQTT.md) · [API reference](API_REFERENCE.md) · [Fault-tolerant temperature sensor](scenarios/fault-tolerant-temperature-sensor.md).
