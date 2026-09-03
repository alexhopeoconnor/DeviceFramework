# Home Assistant Telemetry

This example adds one application parameter and one Home Assistant entity without taking over DeviceFramework’s network or MQTT lifecycle. **Telemetry report interval** appears in the supported configuration surfaces, and **Uptime** becomes a discovered MQTT sensor once Wi-Fi and MQTT are configured.

Provision the board interactively, provide MQTT settings through the normal configuration flow, then inspect the discovered device in Home Assistant. Change the report interval and confirm that the sensor update cadence follows the saved value.

The parameter is keyed by a stable ID, while its label and presentation can evolve. Add a schema migration only when changing the meaning of an already-saved value.

See [configuration](../../docs/CONFIGURATION.md) and the shared [examples guide](../README.md).
