# DeviceFramework documentation

DeviceFramework separates the device-facing workflow from framework development.

| I want to… | Read |
| --- | --- |
| Build a first ESP8266 or ESP32 device | [Getting started](GETTING_STARTED.md) |
| Add another board family or inspect the target layout | [Target organization](TARGETS.md) |
| Understand local profiles, V4 storage, migration, reset, or passwords | [Configuration and profiles](CONFIGURATION.md) |
| Brand the existing web admin UI and provisioning portal | [Web UI and provisioning branding](WEB_UI.md) |
| Check the tested stack and target support | [Compatibility](COMPATIBILITY.md) |
| Compile without a board or run a LAN hardware suite | [Testing](TESTING.md) |
| Work on this library or publish a GitHub release | [Development and releases](DEVELOPMENT.md) |

The framework’s release history is in the [changelog](../CHANGELOG.md). Application firmware should depend on one released DeviceFramework tag rather than declaring WiFiManager, DFTE, or ArduinoHA separately.

## Documentation rules

- The configuration guide owns profile and storage semantics.
- The web UI guide owns product presentation and its boundary with WiFiManager.
- The compatibility guide owns the stack version table.
- The testing and development guides own operational commands.
- The project README is a short landing page, not a duplicate manual.

Back to the [project overview](../README.md).
