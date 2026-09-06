# Scenario: fault-tolerant temperature sensor

## The real device pattern

The maintained temperature-monitor firmware samples a DS18B20 asynchronously,
stores operator-selected interval/resolution/offset settings, exposes them in
the portal and Home Assistant, and publishes both a measurement and sensor
health. It continues to run DeviceFramework when the probe is unplugged,
because provisioning, OTA, diagnostics, and eventual recovery remain valuable.

## Design

```text
parameter: report interval / resolution / calibration offset
                         ↓
change callback sets reconfigure flag
                         ↓
non-blocking sensor state machine reconfigures on a later loop
                         ↓
valid sample → temperature HA sensor
missing/invalid sample → health binary sensor + diagnostic state
```

The application owns sensor timing and validity. DeviceFramework owns the
configuration lifecycle and network path. Neither the sensor driver nor an
unavailable MQTT broker is allowed to prevent `DeviceFramework::loop()` from
running.

## Implementation sequence

1. In `beforeSetup`, register stable parameters such as `sampleinterval`,
   `resolution`, and `offset`. Validate ranges that the sensor and hardware
   actually support.
2. Create long-lived ArduinoHA measurement and health entities, then configure
   their metadata before `DeviceFramework::setup()`.
3. After setup, read the now-loaded parameter values and initialise the sensor
   state machine. Do not wait for Home Assistant or MQTT connectivity.
4. Start a conversion, return to the main loop, and read it only after the
   required conversion interval. This leaves time for the portal, OTA, and
   MQTT/client loops.
5. On a valid sample, apply a bounded/calibrated offset and publish the result.
   On a missing device, disconnected bus, or invalid reading, publish health
   false and retain enough diagnostics to identify the failure.
6. When a parameter changes, set a flag. The next application loop safely
   reconfigures the sensor or alters its reporting period.

## Parameter callback pattern

```cpp
bool sensorConfigChanged = false;

void onParameterChange(const String& id, const String&, const String&) {
    if (id == "sampleinterval" || id == "resolution" || id == "offset") {
        sensorConfigChanged = true;
    }
}

void loop() {
    DeviceFramework::loop();
    if (sensorConfigChanged) {
        sensorConfigChanged = false;
        applySensorConfiguration();  // A bounded local operation.
    }
    advanceTemperatureConversion();
}
```

Avoid `delay()` through the conversion window or a retry loop that holds the
main loop. A sensor failure should be a *state*, not a firmware dead end.

## Test sequence

1. Start with no saved Wi-Fi and verify the portal remains usable.
2. Configure Wi-Fi/MQTT; check device discovery and a changing temperature
   entity.
3. Edit the interval/offset from both supported configuration surfaces and
   confirm persistence through restart.
4. Disconnect the probe. Check that health changes, stale/invalid values are
   handled according to the product policy, and portal/OTA/MQTT remain alive.
5. Reconnect the probe and verify automatic recovery without erasing or
   reprovisioning the board.

Related: [Parameters](../PARAMETERS.md) · [Home Assistant and MQTT](../HOME_ASSISTANT_MQTT.md) · [Troubleshooting](../TROUBLESHOOTING.md).
