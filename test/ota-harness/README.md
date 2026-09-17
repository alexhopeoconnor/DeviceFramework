# DeviceFramework OTA fixture

This is a disposable real-board consumer fixture, not a product example. It
builds immutable A/B firmware identities and exercises two distinct OTA
transports without committing a developer's network or deployment password.

The common bases make the on-device partition contract explicit:

- ESP8266 uses `eagle.flash.4m1m.ld`, which reserves the inactive update area.
- ESP32 uses DeviceFramework's tracked `esp32_ota_4m_no_fs.csv`, with `app0`
  and `app1` OTA slots. Serial-flashing A installs that exact table before B is
  offered to either updater.

Both fixtures target 4 MB boards: the ESP8266 environments use `d1_mini` and
the ESP32 environments use `esp32dev`. Choose a different explicit layout for
a different flash size; do not reuse this OTA contract unchanged.

## Portal HTTP OTA

The portal environments use safe tracked profiles with no station credentials:

```text
esp8266_portal_ota_protected_a / _b
esp8266_portal_ota_open_a / _b
esp32_portal_ota_protected_a / _b
esp32_portal_ota_open_a / _b
```

`protected` uses the non-secret fixture password `ota-portal-password`; `open`
sets the DeviceFramework password empty. That changes the provisioning AP
policy, not authorization for WiFiManager's `/u` route: access to that route
comes from joining the provisioning AP. The fixture-only marker reports its
actual policy, image (`A` or `B`), version, and live updater capacity.

Run it only through the named secondary Wi-Fi adapter:

```bash
./tools/device-ui-hardware ota-portal \
  --platform esp8266 --port /dev/serial/by-id/usb-... \
  --client-interface wlx0123456789ab --profile protected
```

The runner builds and preserves A/B before it touches the board, serial-flashes
A, uses a browser to submit B to the real multipart `/u` form, and requires an
automatic outage plus two fresh B marker observations. It leaves the board in
the clean no-station portal state; its temporary adapter connection is removed
unless `--keep` is requested.

The dedicated adapter is a host-side NetworkManager resource, not an OTA
profile secret. A graphical Polkit session may authorize it directly. A
headless runner validates sudo before flashing and elevates only its named
adapter and generated connection actions; it does not run the browser runner
or store private artifacts as root. See
[`docs/TESTING.md`](../../docs/TESTING.md#networkmanager-authorization) for
the `DFUI_NMCLI_AUTH` behavior.

## Station ArduinoOTA

The station-mode environments intentionally take their profile path from
`DEVICEFRAMEWORK_OTA_PROFILE`:

```text
esp8266_udp_ota_a / _b
esp32_udp_ota_a / _b
esp8266_udp_ota_deferred_a / _b
```

The first two pairs cover normal mDNS-resolved ArduinoOTA. The final
ESP8266-only pair forces DeviceFramework's existing mDNS heap guard to defer
the framework responder, then verifies direct-IP ArduinoOTA without giving
ArduinoOTA a second mDNS owner. The command runner creates the real profile in
a mode-600 temporary file from ignored `test/.env`; it is removed on exit.
For a physical run, its generated profile header and original firmware images
remain only in the runner's mode-700 private artifact directory, because the
firmware necessarily embeds the test Wi-Fi credential.

For compile-only work, use a safe tracked fixture from this directory's parent:

```bash
source tools/lib/platformio.sh
DEVICEFRAMEWORK_OTA_PROFILE=../profiles/ota-lan-protected-fixture.json \
  df_pio run -d test/ota-harness -e esp8266_udp_ota_a
```

For a physical test, use `tools/ota-hardware` instead. It validates mDNS in
normal mode and invokes the selected framework's `espota.py` directly, so it
does not depend on PlatformIO's automatic upload-protocol selection.

ArduinoOTA's TCP firmware stream is a reverse connection from the board to the
host callback port, rather than inbound host traffic to board listener port
8266/3232. The runner defaults to read-only `--firewall check`; use
`--firewall manual` to print the exact route-specific UFW rule or the explicit
`--firewall allow` mode to add one temporary, uniquely tagged board-IP to
host-IP rule. `allow` removes only its own tagged rule after each contract and
records a recovery command in the private run directory if cleanup cannot run;
an incompatible pre-existing deny/reject rule is diagnosed rather than changed.

The disposable fixture clears only its own reset-tracker record before each
boot because serial erase does not clear ESP8266 RTC RAM. The runner then
attaches to serial with inactive reset-control lines instead of manufacturing a
second physical reset. This keeps an interrupted prior fixture from changing
the A/B bootstrap result; it does not change the library's production
rapid-reset recovery behavior.

```bash
./tools/ota-hardware arduino \
  --platform esp8266 --port /dev/serial/by-id/usb-... \
  --env-file test/.env --auth both
```

On a host with active UFW that has no suitable pre-existing callback rule, add
`--firewall allow` to that command. It is intentionally opt-in and UFW-specific;
the default remains portable and read-only.

## Local development overrides

Copy `platformio.local.example.ini` to an ignored
`platformio.local.ini.<machine>` only when testing unpublished sibling
worktrees. The checked-in configuration deliberately resolves release
dependencies; CI compiles the fixture but never claims that a real OTA transfer
occurred.
