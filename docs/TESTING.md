# Testing

`compile` builds the complete Unity test firmware with PlatformIO's `--without-uploading --without-testing`, so it needs no attached microcontroller. Run both supported targets before a release:

```bash
./scripts/run-tests.sh compile --platform esp8266
./scripts/run-tests.sh compile --platform esp32
```

The `hardware` mode runs the integration tests against a connected device and starts the endpoint-fetcher Docker service. It reads required WiFi and MQTT values from an ignored `test/.env`; copy `test/.env.example` and fill it locally. The runner generates an ignored C++ header only for the duration of the run, then removes it.

CI always runs compile-only tests. Hardware tests remain an explicit local gate because they require LAN access, a board, and test credentials.
