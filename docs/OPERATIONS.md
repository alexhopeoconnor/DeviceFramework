# Device operations and recovery

This guide is for the person deploying or maintaining a DeviceFramework
firmware. It describes the operational boundaries the framework owns and the
ones a firmware must make explicit.

## First boot and normal operation

1. Flash a firmware with a stable application ID.
2. On a board without usable saved Wi-Fi, join its provisioning portal and
   supply Wi-Fi plus any device settings. A local profile can instead seed a
   new device; see [Configuration and profiles](CONFIGURATION.md).
3. After a successful save/restart, use the `.local` hostname when multicast
   DNS works on the LAN, or the DHCP address from the router.
4. Configure an MQTT broker in the normal settings flow if the device uses
   Home Assistant. The rest of the firmware remains functional while the
   broker is absent or reconnecting.

The local web interface is compiled only with `ENABLE_WEB_INTERFACE`. It is
not a stable, general-purpose REST API: use it as the framework's existing
operator/admin surface. See [Web UI branding](WEB_UI.md) and
[Web resource limits](WEB_RESOURCES.md).

## Credential and password management

One current device password protects the provisioning AP, Arduino OTA,
authenticated HTTP, and WebSerial. `default1` is a development default, not a
shipping credential. Establish a per-deployment password through an ignored
profile or a trusted device-management flow before production use.

`DeviceFramework::setDevicePassword()` validates and transactionally stores the
new value before it becomes the runtime authority. Restart after a successful
programmatic rotation so already-started transports use the new value:

```cpp
if (DeviceFramework::setDevicePassword("new-local-password")) {
    DeviceFramework::restart(DeviceFrameworkRestartReason::PasswordChanged);
}
```

`restart()` records a one-shot intentional-restart reason, so the next boot
will not mistake it for a physical double reset. `getLastRestartReason()` lets
application code observe that a framework-marked restart occurred; `None` means
there was no framework marker and does not distinguish power loss, button,
watchdog, or other external reset causes.

The value must be empty (intentionally open local access) or 8–31 characters.
If OTA is part of the deployment process, update the ignored local uploader
credential as part of the same rotation. The profile file does not become the
runtime authority just because it contains the matching value.

## Reset scopes

| Action | Wi-Fi profiles | Parameters | Shared password | Typical use |
| --- | --- | --- | --- | --- |
| `reset(WiFiOnly)` | Cleared | Retained | Retained | Move a device to another LAN. |
| `reset(ParametersOnly)` | Retained | Defaults restored | Retained | Recover editable device policy without losing network access. |
| `reset(Factory)` | Cleared | Defaults restored | `default1` restored | Physical recovery/redeployment. |

The framework's reset tracker also uses early restarts: a double reset in the
configured timeout clears Wi-Fi profiles; a triple reset performs a factory
reset and restarts. Treat this as physical recovery behaviour, not an
application safety interlock. A controller must still choose safe output levels
at every boot, before the tracker or Wi-Fi runs.

`addMQTTResetCommand()` and `addMQTTRestartCommand()` can expose custom MQTT
controls. Add them only when the product's broker access, topic policy, and
physical consequences make remote reset acceptable. They are not a substitute
for a safe physical recovery procedure.

## What to capture when diagnosing a field issue

- Firmware/application version and DeviceFramework release tag.
- Board target (`esp8266`/`esp32`), reset cause, and whether portal mode opens.
- Whether the device has a usable IP and whether `.local` resolution is the
  failing part rather than connectivity itself.
- Current MQTT host/port (redact credentials), broker reachability, and Home
  Assistant discovery/state observations.
- The smallest reproducible path: cold boot, portal save, broker restart,
  parameter edit, or an output/fault transition.
- Free heap/largest-block observations for ESP8266 web or mDNS issues.

Do not paste Wi-Fi passwords, MQTT passwords, or a production device password
into an issue, CI log, profile example, or screenshot.

## Release and verification path

Use a released DeviceFramework tag in consuming firmware. Before a library
release, run documentation checks and compile both targets; before deploying a
real device type, add the hardware and Home Assistant checks proportionate to
its risk. The exact repository commands are documented in [Testing](TESTING.md).

Next: [Troubleshooting](TROUBLESHOOTING.md) · [Configuration and profiles](CONFIGURATION.md) · [Protected-output scenario](scenarios/protected-output-controller.md).
