# Scenario: protected output controller

## The real device pattern

Maintained solar/battery and portable-load controllers manage a low-active
relay, voltage/current readings, configurable cutoff/recovery thresholds, and
fault/interlock conditions. They persist operator policy and, where useful,
a compact restart-resilient request. They do **not** equate a command request
with a physically permitted output.

## State model

```text
operator / HA request ─┐
persisted cutoff policy ├─> application safety decision ─> applied output
measured voltage/fault ─┘                                  ↓
                                              HA reports actual state
```

The controller owns the safety decision. DeviceFramework persists editable
policy, exposes a deliberate request interface, and keeps connectivity alive;
it does not know the electrical constraints of a particular relay, battery, or
load.

## Boot sequence

1. Configure the output GPIO and drive the electrically safe level **before**
   `FirmwareIdentity::configure()` or framework setup. Confirm active-high/
   active-low wiring on the real board.
2. Create a native HA switch that represents the applied output and any
   measurement/fault entities. Its command callback changes only an
   application `requestedOn` value.
3. Register cutoff/recovery and other operator-adjustable values as validated
   parameters in `beforeSetup`.
4. Call `DeviceFramework::setup()`, then read the loaded values and perform a
   first safety evaluation. A saved request alone must not energise the output.
5. On each loop, call `DeviceFramework::loop()`, update measured inputs, run
   the safety calculation, change the output only when permitted, and report
   the applied result.

## Safe control shape

```cpp
bool requestedOn = false;
bool appliedOn = false;
bool faultActive = false;

bool mayApplyOutput() {
    const float cutoff = DeviceFramework::getParameterRegistry()
                             .getValueAsFloat("cutoffvoltage");
    return requestedOn && !faultActive && measuredVoltage >= cutoff;
}

void enforceOutput() {
    const bool permitted = mayApplyOutput();
    if (permitted == appliedOn) return;
    appliedOn = permitted;
    digitalWrite(kOutputPin, appliedOn ? kOnLevel : kOffLevel);
    appliedOutputEntity.setState(appliedOn);
}
```

Use product-specific hysteresis/recovery rules rather than the simplified
single threshold above. Crucially, report `appliedOn`, not the requested value,
when a fault or cutoff prevents energisation. This gives the operator a truthful
view and avoids an automation believing a load is running when it is not.

## Optional restart-resilient intent

`DeviceFrameworkRtcBlob` can preserve a compact request across a restart:

```cpp
struct OutputIntent { uint8_t requestedOn; };
OutputIntent intent{requestedOn ? 1 : 0};
DeviceFrameworkRtcBlob::write("load.intent", &intent, sizeof(intent));
```

Read it only after driving the safe GPIO level, validate it, and pass it back
through the same safety calculation. It is not a substitute for persistent
configuration or safe electrical design. On ESP8266, only one blob slot exists
for the entire firmware, so different blob names still overwrite one another.

## Test sequence

1. Verify with a meter/LED/load simulator that reset and power-on always drive
   the safe level before Wi-Fi or MQTT is available.
2. Request output while conditions are safe; verify applied state and HA agree.
3. Introduce a low-voltage/fault condition; verify immediate safe output and
   truthful applied-state reporting while the request remains explicit.
4. Change cutoff/recovery values through the portal and HA; verify validation,
   non-blocking application, and persistence after restart.
5. Restart during an outstanding request; verify recovered intent is still
   blocked until the physical interlock permits it.

The [Protected Output example](../../examples/05-protected-output/) demonstrates
this state separation using a configured demonstration indicator; it is not
mains-controller firmware.

Related: [Lifecycle](../LIFECYCLE.md) · [Parameters](../PARAMETERS.md) · [Operations](../OPERATIONS.md).
