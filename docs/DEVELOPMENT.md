# Development and releases

This guide is for people working on DeviceFramework itself. Application firmware
should depend on a released DeviceFramework tag; it should not declare
WiFiManager, DFTE, or ArduinoHA separately. DeviceFramework's
[`library.json`](../library.json) is the single source of the tested dependency
set.

## Build without hardware

PlatformIO and Python 3 are required. The compile checks build a minimal clean
consumer which declares only DeviceFramework; they do not upload or execute
firmware, so no board or credentials are needed:

```bash
./scripts/run-tests.sh compile --platform esp8266
./scripts/run-tests.sh compile --platform esp8266 --profile-fixture
./scripts/run-tests.sh compile --platform esp32
./scripts/run-tests.sh compile --platform esp32 --profile-fixture
```

CI runs these normal and profile-fixture checks for every push and pull request.
The normal ESP8266 command also checks the web-interface-free configuration.

## Run the connected-device suite

The optional hardware suite exercises WiFi, MQTT, storage, web-interface restart,
and direct HTTP requests from the development host to a connected board. It needs
curl and Avahi for automatic mDNS discovery (or an explicit device address), a reachable MQTT broker, a WiFi network,
and an unused development device. Copy the template;
the real file is ignored and must never be committed:

```bash
cp test/.env.example test/.env
./scripts/run-tests.sh hardware --platform esp8266 --port /dev/ttyUSB0
```

Use `--env-file path/to/file` when the credentials live outside the repository.
The runner creates its generated test header only for the run and removes it on
exit. Profile-fixture builds advertise a unique mDNS name; after Unity completes,
the host verifies authenticated status, root-page, and 404 responses directly. Set
`DEVICEFRAMEWORK_TEST_DEVICE_HOST` in the ignored env file when Avahi or mDNS is unavailable.

## Work against sibling checkouts

Released builds use public Git tags. For local library development, copy
[`platformio.local.example.ini`](../platformio.local.example.ini) to a file such
as `platformio.local.ini.alex`, update the `symlink://` paths, and leave that
file untracked. `platformio.ini` loads matching `platformio.local.ini.*` files
when present, so no tracked configuration or application dependency needs to
change.

## Publish a release

1. Update `library.json`, `CHANGELOG.md`, and documentation for the intended
   semantic version. Keep the compatibility table current when the tested stack
   changes.
2. Run the four compile checks above; run the hardware suite when its covered
   behavior changed.
3. Commit the release preparation, then validate and create the annotated tag:

   ```bash
   ./scripts/prepare-release.sh vMAJOR.MINOR.PATCH --tag
   ```

4. Push the branch and tag. The tag workflow validates the PlatformIO package
   again and creates the GitHub Release from that tag.

The `#vMAJOR.MINOR.PATCH` portion of a PlatformIO Git dependency is a Git ref:
PlatformIO clones the repository and checks out that tag. It does not download
a GitHub Release asset. Tags therefore provide reproducible dependency inputs;
an exact commit SHA is an even stricter pin for short-lived diagnostics.
