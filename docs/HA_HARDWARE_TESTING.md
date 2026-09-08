# Local Home Assistant hardware test harness

This optional local harness tests the actual path that normal firmware tests
cannot cover:

```text
DeviceFramework firmware -> Wi-Fi -> Mosquitto -> Home Assistant
Home Assistant service -> MQTT command -> firmware -> Home Assistant state
```

It uses Docker Compose for Home Assistant, Mosquitto, onboarding, verification,
browser rendering, and USB flashing. A physical run also needs PlatformIO to
build firmware and Python 3 to capture serial output; it does not need a host
`esptool`, browser, Node.js, or Home Assistant installation. The optional
disposable AP mode additionally needs NetworkManager, `ip`, and a dedicated
AP-capable Wi-Fi adapter.

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

Harness sessions and physical/compile operations each take a local advisory
lock. A second invocation in the same checkout waits safely instead of racing a
Docker broker, generated test header, or PlatformIO build tree.

`Ctrl-C` reports that cleanup is running, then removes any temporary resources
created by a non-retained command. If the terminal or host is forcibly stopped
before cleanup can run, use `./tools/ha-hardware down` before starting another
retained session.

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

## Retained session and parallel boards

`up` starts one disposable stack for interactive HA inspection at
`http://127.0.0.1:8125` and prints a session ID. It creates one private,
session-scoped firmware header (mode 0400) from the ignored Wi-Fi input, plus an
ignored PlatformIO override and build directory. The retained state file is mode
0600 under the normal XDG state directory and contains paths only, never
credentials.

Compile each target once, then start one worker per physical board. Workers do
not invoke PlatformIO: each mounts the prebuilt, immutable image into the pinned
Docker `esptool` container, flashes only its assigned USB device, and captures
that board's Unity output. This makes different USB boards safe to run in
parallel without sharing PlatformIO's mutable package or build cache.

```bash
./tools/ha-hardware up --network existing --env-file test/.env --ha-version ui
# Copy the printed session ID into SESSION.
SESSION=dfha1234567890
./tools/ha-hardware compile --session "$SESSION" --platform esp8266
./tools/ha-hardware compile --session "$SESSION" --platform esp32
./tools/ha-hardware worker --session "$SESSION" --alias esp8266-a --platform esp8266 --port /dev/ttyUSB1 &
./tools/ha-hardware worker --session "$SESSION" --alias esp8266-b --platform esp8266 --port /dev/ttyUSB2 &
./tools/ha-hardware worker --session "$SESSION" --alias esp32 --platform esp32 --port /dev/ttyUSB0 &
wait
./tools/ha-hardware verify --session "$SESSION" --device-id DEVICE_ID
./tools/ha-hardware restart --session "$SESSION" --service homeassistant
./tools/ha-hardware verify --session "$SESSION" --device-id DEVICE_ID --mode ha-restart
./tools/ha-hardware restart --session "$SESSION" --service mqtt
./tools/ha-hardware verify --session "$SESSION" --device-id DEVICE_ID --mode mqtt-restart
./tools/ha-hardware visual --session "$SESSION" --device-id DEVICE_ID
./tools/ha-hardware down
```

Each worker writes a Git-ignored `result.json`, `flash.log`, and `serial.log`
under `artifacts/ha-hardware/`. Because this sequence includes the visual
command, `--ha-version ui` selects the reviewed Home Assistant version from
`tests/ha-hardware/ui/visual-versions.env`. For protocol-only retained sessions,
omit it to use rolling `stable`. Docker must be allowed to access the selected
USB device; normal Docker permissions are sufficient, and the worker never asks
for sudo. `down` removes the Compose volumes, the generated override and private
runtime directory, a managed AP, and any temporary UFW rule. It does not restore
board firmware; test boards are expected to be reflashed normally.

`run --platform ... --port ...` remains the single-board convenience command.
Use `--keep` with it to retain its stack after a successful physical test. A
non-retained run captures Compose logs, Home Assistant registries/states, raw
and normalized discovery data, serial output, and a summary before cleaning up.

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

The optional visual test harness uses the same disposable Home Assistant and
Mosquitto stack, plus a pinned headless Playwright container. It has no host
Node.js, browser, or Python dependency. It seeds a harness-owned HA configuration
volume and a small YAML dashboard, so it never merges into a developer's normal HA
configuration. The browser remains on Docker's internal network.

Run the board-free visual test harness first:

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

A retained visual session must be started with `--ha-version ui`; this selects the
reviewed HA version without copying a version string into local configuration.
Any other explicit HA version requires `--update-snapshots` when used with
`fixture --ui-capture`, making screenshot changes reviewed source changes rather
than accidental results of the rolling `stable` image. The ordinary protocol
harness continues to use `stable`.

Back to [testing](TESTING.md) · [development](DEVELOPMENT.md) ·
[documentation](README.md).
