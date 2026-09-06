# Local Home Assistant hardware contract

This optional local harness tests the actual path that normal firmware tests
cannot cover:

```text
DeviceFramework firmware -> Wi-Fi -> Mosquitto -> Home Assistant
Home Assistant service -> MQTT command -> firmware -> Home Assistant state
```

It uses Docker Compose for Home Assistant, Mosquitto, onboarding, and
verification. The host only needs Docker Compose, PlatformIO for a physical
run, and (only for the disposable AP option) NetworkManager, `ip`, and a
dedicated AP-capable Wi-Fi adapter.

The broker has anonymous access deliberately and is bound only to the selected
local interface. It must never be port-forwarded or exposed to the internet.

## First check

From the DeviceFramework repository, with the usual sibling source checkouts
available, run:

```bash
./tools/ha-hardware doctor
```

The default sibling ArduinoHA location is `../arduino-home-assistant`. The
runner uses it both for the generic HA/MQTT testkit and for firmware builds. A
local source run intentionally requires the normal sibling stack containing
ArduinoHA, WiFiManager, and DFTE, rather than silently mixing a local ArduinoHA
with unrelated released dependencies.

Before a physical run, compile the selected E2E firmware without starting Docker
or touching a board:

```bash
./tools/ha-hardware compile --platform esp8266
```

The preflight creates a non-secret temporary test configuration and removes it
when the compile finishes.

## Run on the existing Wi-Fi network

Create the ignored `test/.env` from its example and provide the Wi-Fi SSID and
password. Its MQTT fields are not used by this harness: it creates a temporary
credential file that points the board to the local Docker broker with blank
credentials.

```bash
./tools/ha-hardware run --platform esp32 --port /dev/ttyUSB0 \
  --network existing --env-file test/.env
```

The runner detects the default-route interface and its IPv4 address, binds
Mosquitto to that address on port 1883, and prints no Wi-Fi credentials. Use
`--uplink-interface wlan0` or `--mqtt-bind-ip 192.168.1.20` when automatic
detection is not appropriate. Normal Docker permissions are enough. If UFW is
active, the default `--firewall check` only warns without prompting. Use
`--firewall allow` to add one temporary inbound TCP/1883 rule; it prompts once
for sudo and removes that exact rule at cleanup.

## Run with a dedicated USB Wi-Fi adapter

This leaves the default-route Wi-Fi interface alone. NetworkManager creates a
short-lived WPA2 shared AP, including DHCP/NAT, on the adapter named by
`--ap-interface`; the broker is bound to the AP gateway. It requires sudo for
the temporary NetworkManager connection and, if requested, scoped UFW rules.

```bash
./tools/ha-hardware run --platform esp8266 --port /dev/ttyUSB0 \
  --network managed-ap --ap-interface wlan1 --firewall check
```

The helper refuses to turn the current default-route interface into an AP and
checks advertised AP support when `iw` is available. It removes the exact
NetworkManager connection and any rules it created when the run exits. The
board is intentionally not restored; it is test firmware and will be reflashed
normally later.

## Keep an interactive stack

`up` always retains the stack for manual HA inspection; it listens at
`http://127.0.0.1:8125` by default.

```bash
./tools/ha-hardware up --network existing --env-file test/.env
./tools/ha-hardware down
```

Use `--keep` with `run` for the same behaviour after a successful physical
test. The retained-session record is mode 0600 under the normal XDG state
directory, and `down` removes Compose volumes, a managed AP, and any temporary
existing-network UFW rule. A non-`--keep` run captures `compose.log`, serial
output, Home Assistant registries/states, raw discovery MQTT, normalized
hardware discovery, and a run summary in the Git-ignored
`artifacts/ha-hardware/` directory before it tears down containers and
temporary files.

## What the physical run proves

The compact, web-free firmware fixture is compiled only in `esp8266_ha_e2e` or
`esp32_ha_e2e`. It keeps this Home Assistant/MQTT test focused; the regular
hardware suite continues to cover the device web interface. The fixture exposes one direct ArduinoHA sensor and switch plus DeviceFramework-managed
number, select, and text parameters. The verifier checks all five domains in
Home Assistant's entity and device registries, then exercises:

- Home Assistant switch, number, select, and text services;
- the MQTT command arriving at the board and its state returning to Home
  Assistant; and
- Home Assistant and MQTT broker restarts, including re-discovery from the
  board after the non-persistent test broker restarts.

The board-free fixture under `tests/ha-hardware/fixtures/` is a smaller
retained discovery contract for Docker/CI. It proves the current Home Assistant
discovery shape and retained HA restart behavior without a board. It does not
pretend to replace the physical service-command round trip.

## Visual dashboard contract

The optional visual contract uses the same disposable Home Assistant and
Mosquitto stack, plus a pinned headless Playwright container. It has no host
Node.js, browser, or Python dependency. It seeds a harness-owned HA configuration
volume and a small YAML dashboard, so it never merges into a developer's normal HA
configuration. The browser remains on Docker's internal network.

Run the board-free visual contract first:

```bash
./tools/ha-hardware fixture --ui-capture
```

This replays retained discovery and state for a sensor, switch, number, select,
and text entity; restarts HA and Mosquitto; then captures the rendered dashboard.
A physical run adds the opposite-direction check: Playwright clicks the switch in
HA, the command travels through MQTT to the board, and the verifier waits for the
board's returned `on` state:

```bash
./tools/ha-hardware run --platform esp8266 --port /dev/ttyUSB0 --ui-capture
```

Reviewed baseline PNGs live in `tests/ha-hardware/ui/tests/snapshots/`. Their HA
and Playwright versions are pinned in `tests/ha-hardware/ui/visual-versions.env`.
On a mismatch, Playwright retains expected, actual, diff, trace, and JSON report
artifacts under the ignored `artifacts/ha-hardware/<timestamp>/playwright/` path.
Inspect those artifacts before deliberately replacing a baseline:

```bash
./tools/ha-hardware fixture --ui-capture --update-snapshots
```

An explicit HA version other than the pinned visual version also requires
`--update-snapshots`, making screenshot changes a reviewed source change rather
than an accidental result of the rolling `stable` image. The ordinary protocol
harness continues to use `stable`.

Back to [testing](TESTING.md) · [development](DEVELOPMENT.md) ·
[documentation](README.md).
