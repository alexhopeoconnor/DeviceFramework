# Protected Output

This runnable example models the structure of a solar/load or relay controller
without controlling a consequential load: it drives only the target's configured
demonstration indicator. Do not connect the example unchanged to mains, a
battery charger, a relay, or any other real load.

The ESP8266 target uses its board-defined LED. PlatformIO's generic `esp32dev`
target does not define one, so this example explicitly assumes the common
GPIO 2, active-low DevKit indicator. That is not universal: change
`LED_BUILTIN` and `DF_LED_ACTIVE_LOW` in `platformio.ini` before flashing a
board with another pin, polarity, or no LED.

It demonstrates four rules from maintained protected-output consumers:

1. Set the output to its safe level before framework setup.
2. Keep an operator's request separate from the physically applied state.
3. Persist operator-adjustable policy—here, **Maximum output-on time**—through
   the portal and Home Assistant.
4. Let an interlock/timeout force the applied output off and report that fact
   truthfully, without blocking DeviceFramework's network loop.

After provisioning Wi-Fi and MQTT, Home Assistant discovers the protected
output switch, a timeout-lockout binary sensor, and the maximum-on-time number.
Turn the switch on: the LED represents the applied output. When the configured
maximum expires, the LED turns off and the lockout sensor turns on even though
the original command was a request. Turn the switch off to clear the demo
lockout, then turn it on again.

The default maximum is 60 seconds; set 10–600 seconds from the portal or Home
Assistant. For a quick demo choose 10 seconds. This is a *timeout policy*, not
a complete electrical safety design. A real controller also needs validated
polarity, independent hardware protections where appropriate, real fault and
measurement inputs, recovery/hysteresis policy, and physical power-on testing.

Build it like the other examples:

```bash
pio run -d examples/05-protected-output -e esp8266
pio run -d examples/05-protected-output -e esp32
```

See [the protected-output scenario](../../docs/scenarios/protected-output-controller.md), [lifecycle](../../docs/LIFECYCLE.md), and the shared [examples guide](../README.md).
