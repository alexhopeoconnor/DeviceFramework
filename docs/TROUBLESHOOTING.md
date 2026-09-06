# Troubleshooting

Start with the lifecycle rather than assuming that every visible symptom is an
MQTT problem. A new board, a board with invalid Wi-Fi, and a configured board
with an unavailable broker are three normal but different states.

| Symptom | Likely explanation | Check / next action |
| --- | --- | --- |
| Provisioning portal appears after flash | No usable Wi-Fi profile exists. | This is expected for Portal First. Join it and submit valid Wi-Fi settings. |
| Portal reappears after saving | Candidate Wi-Fi could not be verified or the board was reset/erased. | Check SSID/password, signal, and portal save logs. Use a known-good profile only for controlled bootstrap. |
| Device has Wi-Fi but no MQTT/HA entities | Broker setting/DNS/broker reachability is not ready. | Verify MQTT host/port/credentials. The framework starts MQTT only after stable usable Wi-Fi. |
| HA telemetry never changes | The sketch never calls `setValue`/`setState`, or its application loop is blocked. | Confirm `DeviceFramework::loop()` and the sampling state machine run every pass. |
| HA configuration setting changes but hardware does not | The parameter callback assumes it runs at boot, blocks, or never applies state. | Apply loaded values after `setup()` and make callback work non-blocking. |
| A direct `setValue()` change disappears after reboot | Direct application writes are not auto-persisted. | Call `DeviceFramework::saveParameters()` when a programmatic change should survive reboot. |
| `.local` name does not resolve | LAN/client multicast DNS support is unavailable or ESP8266 mDNS is protecting heap. | Use the DHCP IP to distinguish addressability from name resolution. See the mDNS guard in [Configuration](CONFIGURATION.md). |
| ESP8266 web page returns `503` under concurrent browsing | The streamed-response guard is preserving heap. | Measure heap/largest block and tune only through [Web resource limits](WEB_RESOURCES.md). |
| Password works in one surface but OTA/web fails after rotation | Existing transports still use the old startup configuration. | Call `DeviceFramework::restart()` after `setDevicePassword()` and update the local OTA uploader credential. |
| Output briefly energises on reboot | Hardware safe state occurs too late or requested state is used as applied state. | Drive GPIO safe before framework setup, then evaluate interlocks. See [Protected output](scenarios/protected-output-controller.md). |

## Minimal isolation sequence

1. Flash [Portal First](../examples/01-portal-first/) to prove board, target,
   provisioning, and local web access separately from device logic.
2. Flash [Home Assistant Telemetry](../examples/02-home-assistant-telemetry/)
   with a known broker to prove discovery and an updating entity.
3. Add the consuming device's parameters and native entities, keeping all
   application work non-blocking.
4. Test the specific real-world scenario: failed sensor, broker restart,
   operator configuration change, or an output/fault transition—not merely a
   clean boot.

## Logs and testing

Build with an appropriate `LOG_LEVEL` while diagnosing. Prefer concise state
transition logs (Wi-Fi usable, MQTT connected/disconnected, sensor fault,
requested versus applied output) over log spam inside every loop. On ESP8266,
excessive logging and allocation can change the timing/memory issue you are
trying to observe.

For a repeatable board-free build check use [Testing](TESTING.md).

Next: [Lifecycle](LIFECYCLE.md) · [Operations](OPERATIONS.md) · [Scenario guides](SCENARIOS.md).
