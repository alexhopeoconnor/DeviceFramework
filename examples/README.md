# DeviceFramework examples

These are complete, neutral ESP8266/ESP32 DeviceFramework projects. They deliberately start without real Wi-Fi or MQTT credentials, so a flashed board follows the normal interactive provisioning path instead of exposing private deployment information.

```bash
pio run -d examples/01-portal-first -e esp8266
pio run -d examples/01-portal-first -e esp8266 -t upload
```

## First-device smoke test

After flashing Portal First to a clean board:

1. Join the provisioning network and complete the portal flow.
2. Confirm that the device restarts and reconnects to the configured Wi-Fi network without reopening the portal.
3. Open the local web interface through its `.local` hostname or DHCP address.
4. Optionally configure MQTT, then flash [Home Assistant Telemetry](02-home-assistant-telemetry/) to verify discovery and changing telemetry.

| Example | What it demonstrates |
| --- | --- |
| [Portal First](01-portal-first/) | the smallest full device lifecycle and interactive first boot |
| [Home Assistant Telemetry](02-home-assistant-telemetry/) | a framework parameter plus a changing Home Assistant sensor |
| [Branded Device](03-branded-device/) | one presentation configuration for the existing web UI and provisioning portal |
| [Managed Configuration](04-managed-configuration/) | an optional local profile, safe template, and normal profile-free behaviour |
| [Protected Output](05-protected-output/) | safe boot order, a bounded output policy, and requested versus applied HA state using a configured demo indicator |

Each project references the checked-out DeviceFramework source. A real application should replace the example application ID and firmware version, then use its own local profile only when it wants to pre-seed a board.

Back to the [project overview](../README.md).
