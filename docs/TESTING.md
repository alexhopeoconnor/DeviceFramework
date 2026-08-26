# Testing

`compile` builds a minimal consuming application that declares only
DeviceFramework, so it proves the released package manifest resolves every
library without an attached microcontroller. Run the normal and safe profiled
fixtures for both supported targets before a release:

```bash
./scripts/run-tests.sh compile --platform esp8266
./scripts/run-tests.sh compile --platform esp8266 --profile-fixture
./scripts/run-tests.sh compile --platform esp32
./scripts/run-tests.sh compile --platform esp32 --profile-fixture
```

The `hardware` mode runs the Unity/integration suite against a connected device. It
reads required WiFi and MQTT values from an ignored `test/.env`; copy
`test/.env.example` and fill it locally. The runner generates an ignored C++ header
only for the duration of the run, then removes it. With `--profile-fixture`, the
flashed board has a unique mDNS name and the host directly verifies unauthenticated
rejection plus authenticated status, root-page, and 404 content. The Unity suite also checks that a profile does not overwrite a V3 password rotated through the public API. Automatic `.local` discovery uses `avahi-resolve`; set `DEVICEFRAMEWORK_TEST_DEVICE_HOST` in the ignored env file if Avahi or mDNS is unavailable.

CI always runs both compile-only variants. Hardware tests remain an explicit
local gate because they require LAN access, a board, and test credentials.
