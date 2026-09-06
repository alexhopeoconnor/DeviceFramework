# DeviceFramework documentation

| I want to… | Read |
| --- | --- |
| Build a first ESP8266 or ESP32 device | [Getting started](GETTING_STARTED.md) and [examples](../examples/README.md) |
| Understand startup order, portal mode, and safe hardware boot | [Lifecycle](LIFECYCLE.md) |
| Add durable editable settings | [Parameters](PARAMETERS.md) |
| Add native ArduinoHA entities or custom MQTT commands | [Home Assistant and MQTT](HOME_ASSISTANT_MQTT.md) |
| Look up supported sketch-facing APIs and advanced caveats | [API reference](API_REFERENCE.md) |
| Add another board family or inspect the target layout | [Target organization](TARGETS.md) |
| Use local profiles, migration, reset, or the shared password | [Configuration and profiles](CONFIGURATION.md) |
| Deploy, reset, rotate credentials, or diagnose a device | [Operations](OPERATIONS.md) and [Troubleshooting](TROUBLESHOOTING.md) |
| Tune web work for an unusually constrained or resource-heavy device | [Web resource limits](WEB_RESOURCES.md) |
| Brand the existing web admin UI and provisioning portal | [Web UI and provisioning branding](WEB_UI.md) |
| Follow patterns from maintained sensors, controllers, lights, and product projects | [Real-world scenarios](SCENARIOS.md) |
| Check the tested stack and target support | [Compatibility](COMPATIBILITY.md) |
| Compile without a board or run a LAN hardware suite | [Testing](TESTING.md) |
| Work on this library or publish a GitHub release | [Development and releases](DEVELOPMENT.md) |

The framework’s release history is in the [changelog](../CHANGELOG.md). Application firmware should depend on one released DeviceFramework tag rather than declaring its internal dependencies separately.

Back to the [project overview](../README.md).
