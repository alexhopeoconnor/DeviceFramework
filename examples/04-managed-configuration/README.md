# Managed Configuration

This example uses the same firmware as a normal portal-first device. The difference is optional deployment input: an ignored local profile can seed a new board’s device name, Wi-Fi/MQTT values, and shared device password without committing them to Git.

To try the safe template:

1. Copy `platformio.local.example.ini` to an ignored `platformio.local.ini.<machine>` file.
2. Copy `profiles.example/bootstrap.json` to ignored `profiles.local/workshop.json`.
3. Replace the placeholder password and optionally add a `wifi` object using the [configuration guide](../../docs/CONFIGURATION.md).
4. Build and upload normally.

Omit the local INI entirely to create a normal profile-free firmware. A selected profile that omits `wifi` still seeds its permitted values but deliberately opens interactive Wi-Fi provisioning.

See [configuration and profiles](../../docs/CONFIGURATION.md) and the shared [examples guide](../README.md).
