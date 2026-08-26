# DeviceFramework application configuration and local profiles

Each consuming firmware declares three independent values in `include/FirmwareIdentity.h`:

- `APPLICATION_ID` identifies the firmware family and prevents loading another app’s configuration.
- `FIRMWARE_VERSION` is shown by Home Assistant and the device status API.
- `CONFIGURATION_SCHEMA` changes only when a saved value needs a semantic migration.

Adding, removing, or reordering parameters does not need a schema bump. DeviceFramework V3 stores
parameter values by ID, ignores unknown IDs, and leaves new IDs at their registered defaults. Bump
the schema only for a rename, unit conversion, incompatible interpretation, or another transformation.

## Optional private profile

A profile is an opt-in build input owned by the consuming firmware. Keep a safe template beside that firmware, such as `profiles.example/bootstrap.json`, and keep real values in its ignored `profiles.local/` directory. The ignored `platformio.local.ini.<machine>` beside the firmware only selects a profile, an OTA endpoint, or local sibling libraries. It contains no credentials itself.

```text
my-firmware/
├── platformio.ini                    # tracked release build
├── platformio.local.example.ini      # tracked local selector example
├── profiles.example/bootstrap.json   # tracked safe template
└── profiles.local/home.json          # ignored real profile
```

The generated C++ header lives only under `.pio`; it is not a source file and is removed with the build directory. The library-owned PlatformIO/SCons hook uses only Python’s standard library. Sketches do not declare `extra_scripts`, manage a Python environment, or parse credentials. With no `custom_device_profile` option, it does nothing.

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

`bootstrap` is the safe default for a new device. It applies when storage is empty, when a fully valid V3 record belongs to a different `APPLICATION_ID`, or when a recognised DFC2 record needs replacing. An explicit profile can therefore hand a board from one firmware family to another, or move it from 2.0.x storage to V3, without a preliminary erase. It does not overwrite matching-application configuration, unverified/corrupt V3 data, or a compatible schema that requires an explicit migration.

`reconcile` applies once for each new profile ID or revision and records the attempt before WiFi connects, so it does not rewrite portal-managed values on every boot. Increase `profile.revision` after deliberately changing a reconciliation profile; a failed initial WiFi attempt also counts as an attempt.


### Shared device password and OTA

`device_password` is the profile's **initial seed**, not a second authority. On empty storage, an explicitly selected bootstrap/reconcile profile, or recognised DFC2 data, DeviceFramework applies it before WiFiManager, Arduino OTA, HTTP Basic authentication, and WebSerial start, then writes one CRC-protected V3 record containing both the parameters and active password. A corrupt V3 record receives that password only as a RAM recovery fallback; it is never overwritten automatically.

Once a valid V3 record exists, it restores the active password on every boot and the selected profile does not overwrite it. Rotate it from application code with:

```cpp
if (!DeviceFramework::setDevicePassword("new-local-password")) {
    // The value was invalid or the verified V3 write failed; keep the old password.
}
```

The optional web interface offers the same operation at **System Controls → Device Password** and restarts only after the write is verified. `setDevicePassword()` accepts empty (to intentionally disable local authentication) or 8–31 printable profile-compatible characters. The password remains one shared value for the provisioning AP, OTA, HTTP Basic authentication, and WebSerial.

For `espota`, PlatformIO still needs the current password before it can deliver new firmware. The profile hook supplies `--auth` from the ignored profile's `device_password`; do not add a second `--auth` option. After a runtime rotation, update that same ignored JSON value before the next OTA upload. The next firmware boot keeps the V3 runtime value—matching the profile only lets the uploader authenticate.

## Normal builds, migration, and reset

A normal build has no `custom_device_profile`. It generates no private header and never seeds or overwrites saved configuration. A matching V3 record, including its saved password, loads normally; a valid V3 record for another application remains isolated and the new firmware uses defaults until normal provisioning or an explicit profile provides values. A DFC2 record is recognised but neither decoded nor silently erased; select a profile to replace it with fresh V3 data. Use fresh profile-free mode only for an intentionally open local device or interactive provisioning.

Adding, removing, or reordering parameter IDs requires no schema change. Bump the application schema only for a semantic conversion and supply a `DeviceFrameworkConfigMigrationCallback`; return `false` when a prior schema cannot be transformed safely. A firmware never loads a stored schema newer than itself.

**Reset Configuration** retains WiFi credentials and the active device password while restoring framework parameters to defaults. **Factory Reset** clears WiFi credentials, the device password, and both transactional configuration slots. After the restart, a selected profile can seed a new V3 record; without one, normal provisioning creates the new configuration. DeviceFramework 2.1.0 intentionally has no V1/V2 value decoder, mapper, or implicit migration path.
