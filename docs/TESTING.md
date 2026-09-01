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

The `hardware` mode runs the Unity/integration suite against a connected device. It
reads required WiFi and MQTT values from an ignored `test/.env`; copy
`test/.env.example` and fill it locally. The runner generates an ignored C++ header
only for the duration of the run, then removes it. With `--profile-fixture`, it
runs Unity with a bootstrap profile, injects an RTS-only serial reset after the
selected board’s esptool upload, and requires a non-empty zero-failure result.
This keeps USB-UART adapters from producing a false green result when PlatformIO’s
non-interactive monitor does not reset them.

It then flashes a minimal, separate consuming application with a distinct
one-time reconcile profile. This proves a real application can accept provisioned
WiFi after Unity has left a valid V4 record, rather than relying on an erased board.
The runner waits up to 45 seconds for the unique mDNS name, then checks
unauthenticated rejection plus authenticated status, root-page, static assets,
and 404 responses. Finally it checks that the profiled password persists across
the password endpoint’s reboot. Set `DEVICEFRAMEWORK_TEST_DEVICE_HOST` in the
ignored env file to use a known IP instead of Avahi/mDNS.

CI always runs both compile-only variants. Hardware tests remain an explicit
local gate because they require LAN access, a board, and test credentials.

Back to [documentation](README.md) · [project overview](../README.md).
