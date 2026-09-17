# Testing

Run the complete board-free local suite as one interruptible command:

```bash
./scripts/test-nonhardware.sh
```

It checks generated web assets once, then runs both consumer-build fixtures on
ESP8266 and ESP32, all examples, documentation, and the Docker-backed Home
Assistant visual fixture. `Ctrl-C` stops this single sequence without starting
a later check. The individual commands below remain useful while iterating.

The physical contracts deliberately use different network paths. The secondary
USB adapter exists only for captive-portal work; normal DeviceFramework and
ArduinoOTA coverage stay on the host's ordinary LAN route.

All maintained DeviceFramework test and hardware runners use a dedicated
PlatformIO Core/package/cache graph by default, rather than inheriting an
unrelated project's global package metadata. See [PlatformIO toolchains and
package cache](TOOLCHAINS.md#inspecting-and-repairing-the-shared-package-store)
to relocate it or make a disposable diagnostic graph.

| Contract | Transport | Host adapter | Secret source | Required proof |
| --- | --- | --- | --- | --- |
| Normal DeviceFramework hardware | Station Wi-Fi + HTTP | Normal LAN | ignored `test/.env` | Unity, protected routes, `.local` resolution |
| DeviceFramework portal OTA | WiFiManager `POST /u` | Named secondary adapter | tracked safe portal profile | A → browser upload B → automatic reboot → B twice |
| DeviceFramework ArduinoOTA | UDP invitation + reverse TCP stream | Normal LAN | ignored Wi-Fi env + safe fixture password | mDNS hostname upload A → B |
| ESP8266 deferred-mDNS ArduinoOTA | UDP invitation + reverse TCP stream | Normal LAN | ignored Wi-Fi env + safe fixture password | mDNS absent by design; direct-IP A → B still succeeds |

`compile` builds a minimal consuming application that declares only
DeviceFramework, so it proves the released package manifest resolves every
library without an attached microcontroller. Run the normal and safe profiled fixtures for both supported targets before a release:

```bash
./scripts/test.sh compile --platform esp8266
./scripts/test.sh packages --platform esp8266
./scripts/test.sh compile --platform esp8266 --profile-fixture
./scripts/test.sh compile --platform esp32
./scripts/test.sh packages --platform esp32
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
reset them. After Unity, normal and profiled modes each flash a separate minimal
consumer smoke image before the LAN checks. (`--ha-e2e` retains its Unity image
because its caller consumes the exported live device identity.) The smoke image
must report its immutable `0.0.0-hardware-smoke` marker, so an oversized Unity
image or a stale board cannot masquerade as a passing mDNS/web result.

The profiled mode uses a distinct one-time reconcile profile and a different
device password; normal mode uses its own reconcile profile with the stable
`<platform>-controller` hostname and `default1` password. This proves a real
application can accept provisioned WiFi after Unity has left a valid V4 record,
rather than relying on an erased board. Compile-only profile checks also cover a
valid profile with no `wifi` object, ensuring a profiled firmware can deliberately
open interactive provisioning without a dummy SSID.

The runner waits up to 45 seconds for the unique mDNS name through both Avahi
and the host system resolver, then uses that hostname for the HTTP checks:
unauthenticated rejection plus authenticated status, root-page, static assets,
and 404 responses. Finally it checks that the profiled password persists across
the password endpoint’s reboot. Normal hardware coverage intentionally rejects
an IP override: a passing run must prove mDNS. Use the explicit ArduinoOTA
diagnostic mode below only when investigating a failed mDNS result.


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

### Portal HTTP OTA on a real board

The portal browser contract also has an explicit OTA mode. It is separate from
DeviceFramework's UDP ArduinoOTA service: this one exercises WiFiManager's
actual multipart `POST /u` route through the DeviceFramework-branded portal.
It needs an unused board, one serial port, Docker/Playwright, and the same
dedicated secondary Wi-Fi adapter as the regular portal test. It does **not**
read `test/.env`, join the normal LAN, or alter the host default route.

```bash
./tools/device-ui-hardware ota-portal \
  --platform esp8266 \
  --port /dev/serial/by-id/usb-... \
  --client-interface wlx0123456789ab \
  --profile protected

./tools/device-ui-hardware ota-portal \
  --platform esp8266 \
  --port /dev/serial/by-id/usb-... \
  --client-interface wlx0123456789ab \
  --profile open
```

Each invocation builds deterministic A and B images, checks B against the
running A image's real update capacity, erases and serial-flashes A, joins the
test portal only through the named adapter, and submits B through the browser's
real update form. A pass requires all of the following:

- A reports its immutable `0.0.0-portal-ota-a` marker before upload.
- The browser receives WiFiManager's successful `POST /u` JSON response.
- The portal becomes unavailable without a host-triggered reset.
- The portal returns automatically and twice reports immutable image B,
  `0.0.0-portal-ota-b`.

`protected` uses the tracked, non-secret `ota-portal-password` test password for the
provisioning AP. `open` uses a separate tracked profile with an intentionally
empty DeviceFramework password. The fixture reports which policy it actually
booted, so a stale or incorrectly profiled image cannot masquerade as either
case. These modes prove AP policy plus portal update behavior; WiFiManager's
`/u` route itself does not have a separate HTTP Basic-auth layer.

The temporary portal connection is still `never-default`, but OTA mode enables
reconnect on that one disposable connection so the browser can observe the AP
after the board restarts. The command removes it afterward and leaves the
selected board running B in portal mode. Add `--keep` only when retaining that
adapter connection for diagnosis, then run `./tools/device-ui-hardware down`.
Never use a manual reset to turn a failed automatic-reboot observation into a
passing result.

Run the full portal matrix on real hardware before calling this surface covered:
ESP8266 protected, ESP8266 open, ESP32 protected, and ESP32 open. Keep only one
same-platform portal fixture powered during a run; the test AP name is stable
so the named serial board remains the authoritative fixture identity.

### ArduinoOTA on a real station network

`./tools/ota-hardware` covers the separate ArduinoOTA transport: a UDP
invitation to port 8266 (ESP8266) or 3232 (ESP32), followed by the board's
reverse TCP firmware connection to the host. It uses the normal LAN route and
does not touch the secondary portal adapter. Copy `test/.env.example` to the
ignored `test/.env`, add the local Wi-Fi values, then run:

```bash
./tools/ota-hardware doctor --platform esp8266
./tools/ota-hardware arduino \
  --platform esp8266 \
  --port /dev/serial/by-id/usb-... \
  --env-file test/.env \
  --auth both
```

The runner consumes only `DEVICEFRAMEWORK_TEST_WIFI_SSID` and
`DEVICEFRAMEWORK_TEST_WIFI_PASSWORD`; other values in the existing
`DEVICEFRAMEWORK_TEST_*` namespace are ignored so the same local file remains
usable by the normal hardware and fixture runners.

The runner writes a mode-600 temporary profile containing the ignored Wi-Fi
credentials and a safe fixed OTA password, builds immutable A/B artifacts,
serial-erases and flashes A, and then requires a real A-to-B automatic reboot.
Because the compiled profile and firmware necessarily embed the local Wi-Fi
credential, the runner places its generated header, original build outputs,
logs, and retained A/B artifacts below a mode-700 run directory (with files
mode 600). Treat the printed artifact directory as private; use a private
`--output` location if relocating it. The source profile and temporary
PlatformIO override are deleted on exit, while the private run directory is
retained for failure diagnosis.
For protected mode it proves anonymous HTTP is rejected, a deliberately wrong
ArduinoOTA password is rejected, and the correct password completes. Open mode
proves the corresponding unauthenticated HTTP and no-OTA-password path. It
leaves B and the test Wi-Fi profile on the named board for inspection; erase
only on an explicit request:

```bash
./tools/ota-hardware cleanup --platform esp8266 --port /dev/serial/by-id/usb-...
```

A normal pass requires Avahi *and* the host system resolver to agree on
`df-ota-<platform>.local` both before the upload and again after B has rebooted;
the actual uploader receives that hostname, not its resolved IP. The runner
selects the matching normal-route source address and checks that it can bind the
callback TCP port, but never alters the firewall.
If the UDP invitation gets no reply, investigate the listener, Wi-Fi/VLAN, or
client isolation. If authentication succeeds but no TCP callback arrives,
investigate the host firewall, reverse route, or client isolation.

An IP mode exists solely for diagnosis and prints that mDNS was not exercised:

```bash
./tools/ota-hardware arduino \
  --platform esp8266 --port /dev/serial/by-id/usb-... \
  --device-ip 192.168.1.44
```

The ESP8266-only deferred-mDNS fixture uses a distinct hostname and deliberately
raises the existing heap thresholds before startup. It must not resolve by
Avahi or the system resolver during bounded post-boot and post-update checks,
while direct-IP ArduinoOTA must still complete:

```bash
./tools/ota-hardware arduino \
  --platform esp8266 --port /dev/serial/by-id/usb-... \
  --device-ip 192.168.1.44 --defer-mdns
```

This is the hardware proof that `ArduinoOTA.begin(false)` keeps its UDP
listener alive without becoming a second mDNS owner. It is not a substitute for
the normal hostname-resolved test.

An ESP32 may move its single radio to the station network's channel while
joining Wi-Fi. In that case the browser's portal connection can disappear before
it receives the final save response. The harness accepts that transport change
only after the selected test hostname resolves and the post-handoff web checks
pass; a rejected or failed station association still fails the run.

## Refresh README media

README media is an explicit ESP32-only capture, not part of normal tests or
CI. It runs the same real portal-to-station-to-web browser contract and writes
candidate PNGs, videos, reports, and a manifest under the ignored
`artifacts/readme-media/` directory by default:

```bash
./tools/device-ui-hardware full \
  --platform esp32 \
  --port /dev/serial/by-id/usb-... \
  --client-interface USB_WIFI_ADAPTER \
  --capture-readme-media
```

The capture records the branded provisioning portal, connected Device Status,
WebSerial, and Controls journey. Review the printed artifact directory, or use
`--output DIRECTORY` to retain it elsewhere. Promote the approved web assets
only after review:

```bash
./tools/promote-readme-media \
  --from artifacts/readme-media/TIMESTAMP-esp32 \
  --replace
./scripts/check-docs.sh
```

ESP8266 remains covered by the normal real-board tests but does not generate
duplicate documentation media. The Home Assistant screenshot is captured
separately by its pinned fixture. The command above preserves the existing HA
image; when creating it for the first time or refreshing it, follow [Home
Assistant hardware testing](HA_HARDWARE_TESTING.md#native-device-page-evidence)
and add its `--ha-from artifacts/readme-media/ha-TIMESTAMP` argument to the
promotion command.

The renderer preserves the real portal and web recordings but presents the GIF
at 1.25× duration and 5 fps, with deliberate pauses only in the media-only
recording. Normal browser-contract timing and assertions are unchanged. The
Docker renderer validates the GIF duration. The promotion command validates the
successful capture manifest, media type, and size; it never copies raw videos,
browser reports, or arbitrary artifact files.

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
