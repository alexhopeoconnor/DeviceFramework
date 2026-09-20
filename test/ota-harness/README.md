# DeviceFramework OTA fixture

This is a disposable real-board consumer fixture, not a product example. It
builds immutable A/B firmware identities and exercises two distinct OTA
transports without committing a developer's network or deployment password.

The common bases make the on-device partition layout explicit:

- ESP8266 uses `eagle.flash.4m1m.ld`, which reserves the inactive update area.
- ESP32 uses DeviceFramework's tracked `esp32_ota_4m_no_fs.csv`, with `app0`
  and `app1` OTA slots. Serial-flashing A installs that exact table before B is
  offered to either updater.

Both fixtures target 4 MB boards: the ESP8266 environments use `d1_mini` and
the ESP32 environments use `esp32dev`. Choose a different explicit layout for
a different flash size; do not reuse this OTA test-harness configuration unchanged.

## Portal HTTP OTA

The portal environments use safe tracked profiles with no station credentials:

```text
esp8266_portal_ota
esp32_portal_ota
```

Each environment receives either the safe tracked `protected` or `open`
profile through `DEVICEFRAMEWORK_OTA_PROFILE`; it is built twice for that
profile by the named test-harness scripts: first with a generated A identity,
then—after A is copied aside—with B. The identity header is placed in an
ignored, owner-only directory unique to that run and included only by the
fixture's `main.cpp`. The two genuinely different images therefore reuse the
environment's dependency objects without a second dependency directory, while
one run can never replace another run's A/B build input. The scripts remove the
safe generated header on exit.

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

The runner builds and preserves A, serial-flashes it, then builds and preserves
B in the same environment before a browser submits B to the real multipart
`/u` form. It requires an automatic outage plus two fresh B marker
observations. A passive PySerial
recorder stays attached from A through B with DTR/RTS inactive for diagnostic
evidence; it never manufactures a reset and its product-log wording is not a
pass/fail condition. It leaves the board in the clean no-station portal state;
its temporary adapter connection is removed unless `--keep` is requested.

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
esp8266_udp_ota
esp32_udp_ota
esp8266_udp_ota_deferred
```

The first two pairs cover normal mDNS-resolved ArduinoOTA. The final
ESP8266-only pair forces DeviceFramework's existing mDNS heap guard to defer
the framework responder, then verifies direct-IP ArduinoOTA without giving
ArduinoOTA a second mDNS owner. The command runner writes the real profile from
ignored `test/.env` to a stable, `0600` private path below the ignored
per-platform, `0700` `.pio/ota-hardware-profiles/<platform>/` directory,
then removes it on exit. Its parent directory may be `0755` because it
contains no credentials. This must be separate from PlatformIO's build root,
which it clears
before profile generation. The stable path prevents a fresh temporary filename
from invalidating every PlatformIO object; only profile-dependent sources
rebuild. The separate private build cache and generated profile header embed
the test Wi-Fi credential. The per-run artifact directory still keeps its own
copied A/B images and logs.

For all safe board-free A/B pairs on one target, use the maintained command:

```bash
./scripts/test-ota-fixtures.sh --platform esp8266
```

For a physical test, use `tools/ota-hardware` instead. It builds and preserves
both immutable images before it erases the explicitly named board, then rebuilds
and serial-flashes A and attaches its passive recorder immediately. It assigns a
compact run-unique mDNS name, validates it through Avahi and the host resolver
in normal mode, and invokes the selected framework's `espota.py` directly, so
it does not depend on PlatformIO's automatic upload-protocol selection. The
serial record is retained for diagnosis while the required evidence is the real
espota transfer, automatic outage, B marker responses, and the appropriate
resolver behaviour. The post-B resolver recheck is useful reachability evidence
but is not presented as cache-proof multicast proof.

ArduinoOTA's TCP firmware stream is a reverse connection from the board to the
host callback port, rather than inbound host traffic to board listener port
8266/3232. The runner never changes host firewall policy; a blocked callback
is an environment diagnosis, not a substitute for the real upload test-harness run.

The physical runner explicitly erases the named board before it flashes A. The
disposable fixture clears only its own RTC tracker because serial erase does
not clear ESP8266 RTC RAM. It deliberately does not reset transactional storage
after framework initialisation: the erased board already provides the required
fresh storage state, while a late fixture-only write can disrupt the ESP8266
portal DHCP path. The runner then attaches to serial with inactive reset-control
lines instead of manufacturing a second physical reset. This keeps fixture
bootstrap deterministic without changing the library's production rapid-reset
recovery behavior.

```bash
./tools/ota-hardware arduino \
  --platform esp8266 --port /dev/serial/by-id/usb-... \
  --env-file test/.env --auth both
```

## Local development overrides

Copy `platformio.local.example.ini` to an ignored
`platformio.local.ini.<machine>` only when testing the checked-out
DeviceFramework source. It gives that source its own persistent build directory
while it uses the exact released dependencies declared by `library.json`; this
keeps consumer dependency ownership unambiguous and avoids resolving a pinned
release alongside a sibling substitute. It never clears the shared package
cache. Test an unpublished WiFiManager change through WiFiManager's own portal
harness, then run this fixture after DeviceFramework adopts the released
WiFiManager version. The hardware runner reads the effective PlatformIO build
directory, so the selector works for physical A/B tests too. CI compiles the
fixture but never claims that a real OTA transfer occurred.
