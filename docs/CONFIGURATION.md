# DeviceFramework application configuration and local profiles

Each consuming firmware declares three independent values in `include/FirmwareIdentity.h`:

- `APPLICATION_ID` identifies the firmware family and prevents loading another app’s configuration.
- `FIRMWARE_VERSION` is shown by Home Assistant and the device status API.
- `CONFIGURATION_SCHEMA` changes only when a saved value needs a semantic migration.

Adding, removing, or reordering parameters does not need a schema bump. DeviceFramework 2 stores
parameter values by ID, ignores unknown IDs, and leaves new IDs at their registered defaults. Bump
the schema only for a rename, unit conversion, incompatible interpretation, or another transformation.

## Optional private profile

A profile is an opt-in build input. It is not committed and it never writes a secrets file into the
firmware project. Create the ignored directory and copy the safe template:

```bash
mkdir -p device-profiles.local
# copy a repository-specific non-secret profile example into device-profiles.local/
# copy the consuming project's platformio.local.example.ini to an ignored platformio.local.ini.<machine>
```

Set real WiFi, MQTT, and device-password values in `device-profiles.local/your-firmware.json`.
The generated header lives only under `.pio`; it is not a source file and is removed with the build
directory. `platformio.local.ini.*` and `device-profiles.local/` are ignored by Git.

The profile compiler accepts this strict JSON shape:

```json
{
  "format": 1,
  "application": "your-firmware",
  "profile": { "id": "home-temperature", "revision": 1, "policy": "bootstrap" },
  "device_password": "8-to-31-character-password",
  "wifi": { "ssid": "network", "password": "network-password" },
  "parameters": { "mqttserver": "mqtt.local", "mqttport": "1883" }
}
```

`bootstrap` seeds only a device whose DeviceFramework storage is empty. `reconcile` applies once
for every new profile ID or revision; it records both an attempt and a successful WiFi connection.
It therefore does not overwrite portal-managed values on every boot. Increase `profile.revision` to
deliberately apply a changed reconciliation profile.

The DeviceFramework PlatformIO package owns the Python profile compiler. It uses only Python’s
standard library and is run by PlatformIO/SCons; sketches do not declare `extra_scripts` or manage a
separate Python environment. With no `custom_device_profile` option, it does nothing.

## Reset and migration behavior

- **Reset Configuration** retains WiFi credentials and resets framework parameters to their defaults.
- **Factory Reset** clears WiFi credentials and both transactional configuration slots.
- A V1 positional EEPROM record is imported once when it exactly matches the known legacy layout, then
  rewritten as V2. V1 import is intentionally bounded and will be removed in a later major release.
- A saved schema newer than the firmware is not loaded. A firmware schema newer than the saved schema
  requires the application migration callback to approve the transformation.

Normal builds need no profile. This keeps OTA upgrades safe: an unprofiled new firmware reads the
