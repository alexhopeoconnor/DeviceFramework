# Testing

`compile` builds a minimal consuming application that declares only
DeviceFramework, so it proves the released package manifest resolves every
library without an attached microcontroller. Run the normal and safe profiled fixtures for both supported targets before a release:

```bash
./scripts/test.sh compile --platform esp8266
./scripts/test.sh compile --platform esp8266 --profile-fixture
./scripts/test.sh compile --platform esp32
./scripts/test.sh compile --platform esp32 --profile-fixture
```

With `--profile-fixture`, the consumer build checks three deliberately different profile contracts: bootstrap with Wi-Fi, bootstrap without Wi-Fi, and reconciliation with explicit managed parameters. The framework test target separately proves that a changed reconcile profile ID or revision overwrites only the supplied values and preserves omitted runtime values.

The `hardware` mode runs the Unity/integration suite against a connected device. It
reads required WiFi and MQTT values from an ignored `test/.env`; copy
`test/.env.example` and fill it locally. The runner generates an ignored C++ header
only for the duration of the run, then removes it. Hardware invocations take an
exclusive local lock before they generate that header or touch the shared PlatformIO
build tree, so a second local hardware run waits rather than racing the active one.
With `--profile-fixture`, it runs Unity with a bootstrap profile, owns the serial
port while it injects an RTS-only reset after the selected board’s esptool upload,
and requires a non-empty zero-failure result. This keeps USB-UART adapters from
producing a false green result when PlatformIO’s non-interactive monitor does not
reset them.

It then flashes a minimal, separate consuming application with a distinct
one-time reconcile profile and a different device password. This proves a real
application can accept provisioned WiFi and deliberately rotate the shared
password after Unity has left a valid V4 record, rather than relying on an
erased board. Compile-only profile checks also cover a valid profile with no
`wifi` object, ensuring a profiled firmware can deliberately open interactive
provisioning without a dummy SSID.

The runner waits up to 45 seconds for the unique mDNS name, then checks
unauthenticated rejection plus authenticated status, root-page, static assets,
and 404 responses. Finally it checks that the profiled password persists across
the password endpoint’s reboot. Set `DEVICEFRAMEWORK_TEST_DEVICE_HOST` in the
ignored env file to use a known IP instead of Avahi/mDNS.


For a Docker-backed Home Assistant/Mosquitto test of a physical board, use the [local HA hardware contract](HA_HARDWARE_TESTING.md). It uses the same ignored Wi-Fi credentials, but supplies an isolated anonymous MQTT broker and verifies Home Assistant discovery, services, return state, and restarts. Its Docker-only retained fixture is also the intentionally small CI coverage for current HA behavior.

For a deterministic current-HA dashboard check with no board, run `./tools/ha-hardware fixture --ui-capture`. A physical `run ... --ui-capture` additionally proves a headless browser switch action reaches the board and returns through MQTT; see the hardware guide for reviewed screenshot baselines and failure artifacts.

Run `./scripts/check-docs.sh` after changing Markdown, examples, or generated web assets. It verifies local documentation links, required guides, web assets, and that every numbered example remains a buildable project shape.

CI always runs both compile-only variants. Hardware tests remain an explicit
local gate because they require LAN access, a board, and test credentials.

Back to [documentation](README.md) · [project overview](../README.md).
