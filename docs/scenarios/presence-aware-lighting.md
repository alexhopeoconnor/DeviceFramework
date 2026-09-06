# Scenario: presence-aware lighting

## The real device pattern

The maintained dual-channel LED/presence controller combines two native
ArduinoHA lights, presence/motion health, and a radar peripheral whose
thresholds and modes are editable from Home Assistant. Dimmable-light
consumers add PWM, local sensors, and product-specific effects. These sketches
need responsive network servicing while a peripheral may require a multi-step,
serial/I²C configuration operation.

## Design

```text
portal or HA parameter write
            ↓
short registry callback records pending configuration
            ↓
loop advances one peripheral/configuration state-machine step
            ↓
native light and sensor entities publish applied/observed state
```

Use a framework parameter for durable peripheral configuration—threshold,
timeout, select mode, or sensitivity. Use native `HALight`/sensor entities for
live light/presence behaviour. Custom effect commands are optional and should
be parsed into an application request, not directly perform a long operation in
the MQTT callback.

## Implementation rules

1. Construct native light/sensor entities at long-lived scope, set metadata,
   and register their callbacks before framework setup.
2. In `beforeSetup`, register every editable peripheral setting with matching
   validation and portal/HA presentation.
3. Register one parameter change callback that marks relevant fields dirty.
4. After `setup()`, apply the loaded settings once and start the peripheral
   state machine.
5. In `loop()`, call `DeviceFramework::loop()` first, then service at most a
   bounded portion of the peripheral transaction. Publish actual application
   state after it changes.

## Example dirty-state pattern

```cpp
bool radarReconfigurePending = false;

void onParameterChange(const String& id, const String&, const String&) {
    if (id == "radarthreshold" || id == "radartimeout" || id == "radarmode") {
        radarReconfigurePending = true;
    }
}

void loop() {
    DeviceFramework::loop();
    if (radarReconfigurePending && radar.isIdle()) {
        radarReconfigurePending = false;
        radar.beginConfiguration(readRadarSettings());
    }
    radar.advance();
    updateLightAndPresenceEntities();
}
```

If an application needs a command outside the normal HA light contract, use a
custom DeviceFramework MQTT handler after setup and validate its payload. Keep
the handler bounded exactly like a parameter callback.

## Test sequence

1. Verify local light control and presence reporting with MQTT unavailable.
2. Change a number/select setting from Home Assistant; confirm it persists and
   peripheral configuration advances without portal/MQTT starvation.
3. Change the same setting in the portal and verify consistent validation.
4. Disconnect/reconnect MQTT; verify discovery and actual light/presence state
   return without recreating entities.
5. Exercise an effect/command during a busy peripheral update and confirm one
   request does not corrupt or interleave the other.

Related: [Home Assistant and MQTT](../HOME_ASSISTANT_MQTT.md) · [Parameters](../PARAMETERS.md) · [API reference](../API_REFERENCE.md).
