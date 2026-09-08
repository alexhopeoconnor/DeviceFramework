# Testing

Run the complete board-free local suite as one interruptible command:

```bash
./scripts/test-nonhardware.sh
```

It checks generated web assets once, then runs both consumer-build fixtures on
ESP8266 and ESP32, all examples, documentation, and the Docker-backed Home
Assistant visual fixture. `Ctrl-C` stops this single sequence without starting
a later check. The individual commands below remain useful while iterating.

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
only for the duration of the run, then removes it. Every `scripts/test.sh` mode
takes one exclusive local lock before touching PlatformIO's package/build state;
hardware mode additionally uses it before generating that header. A second local
test waits rather than racing the active one.
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


For a Docker-backed Home Assistant/Mosquitto test of physical boards, use the [local HA hardware test harness](HA_HARDWARE_TESTING.md). It uses the same ignored Wi-Fi credentials, supplies an isolated anonymous MQTT broker, and verifies Home Assistant discovery, services, returned state, and restarts. A retained session compiles each target once and lets Docker-contained flashing workers test separate USB boards in parallel without a host `esptool`; its Docker-only retained fixture is the intentionally small CI coverage for current HA behavior.

For a deterministic current-HA dashboard check with no board, run `./tools/ha-hardware fixture --ui-capture`. A physical `run ... --ui-capture` additionally proves a headless browser switch action reaches the board and returns through MQTT; see the hardware guide for reviewed screenshot baselines and failure artifacts.

## Browser evidence on a real board

The WiFiManager repository tests its portable portal on its own. DeviceFramework
also tests the integration boundary: it flashes a DeviceFramework fixture with no
saved Wi-Fi configuration, so the framework starts and brands the real
WiFiManager portal. A Docker-contained Playwright browser then checks the portal
launch, Wi-Fi scan, configuration save, station hand-off, and the framework's
own status, serial, controls, and about pages.

Use a dedicated USB Wi-Fi adapter for the portal connection. The command refuses
the host default-route adapter and marks the portal connection `never-default`,
so normal LAN and internet traffic stay on the primary adapter:

```bash
./tools/device-ui-hardware doctor --client-interface wlx0123456789ab
./tools/device-ui-hardware full --platform esp8266 --port /dev/serial/by-id/… \
  --client-interface wlx0123456789ab \
  --output ~/Desktop/DeviceFramework-Visual-Harness/$(date +%F)/esp8266
```

`full` erases and reflashes only the named board, runs the real portal-to-station
journey, and leaves screenshots, browser diagnostics, and a credential-free
manifest in the supplied directory. It reads the ignored `test/.env` only at
runtime; credentials are never written into artifacts or source control. Use
`portal` for only the provisioning surface, `web --url http://device.local` for
an already-running web UI, and `down` to remove a retained secondary-adapter
portal connection.

An ESP32 may move its single radio to the station network's channel while
joining Wi-Fi. In that case the browser's portal connection can disappear before
it receives the final save response. The harness accepts that transport change
only after the selected test hostname resolves and the post-handoff web checks
pass; a rejected or failed station association still fails the run.

Run `./scripts/check-docs.sh` after changing Markdown, examples, or generated web assets. It verifies local documentation links, required guides, web assets, and that every numbered example remains a buildable project shape.

CI always runs both compile-only variants. Hardware tests remain an explicit
local gate because they require LAN access, a board, and test credentials.

`./scripts/test-nonhardware.sh` deliberately compiles the consumer fixture with
`test/compile-project/platformio.release.ini`. That configuration excludes every
ignored `platformio.local.ini.*` override while still consuming the checked-out
DeviceFramework source. It therefore proves the current framework works with
the exact first-party library tags declared in `library.json`; local sibling
worktrees cannot accidentally make a release gate pass.

Back to [documentation](README.md) · [project overview](../README.md).
