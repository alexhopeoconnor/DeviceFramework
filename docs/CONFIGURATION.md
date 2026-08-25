# DeviceFramework application configuration and local profiles

Each consuming firmware declares three independent values in `include/FirmwareIdentity.h`:

- `APPLICATION_ID` identifies the firmware family and prevents loading another app’s configuration.
- `FIRMWARE_VERSION` is shown by Home Assistant and the device status API.
- `CONFIGURATION_SCHEMA` changes only when a saved value needs a semantic migration.

Adding, removing, or reordering parameters does not need a schema bump. DeviceFramework 2 stores
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

`bootstrap` is the safe default for a new device. It applies when storage is empty or when a fully valid V2 record belongs to a different `APPLICATION_ID`; an explicit profile can therefore hand a board from one firmware family to another without a preliminary erase. It does not overwrite matching-application configuration or unverified/corrupt data.

`reconcile` applies once for each new profile ID or revision and records the attempt before WiFi connects, so it does not rewrite portal-managed values on every boot. Increase `profile.revision` after deliberately changing a reconciliation profile; a failed initial WiFi attempt also counts as an attempt.


### Shared device password and OTA

`device_password` is intentionally separate from the stored parameter configuration. A selected profile installs it into runtime memory on **every boot**, before WiFiManager, Arduino OTA, HTTP Basic authentication, and WebSerial start. Profile policy controls only WiFi and parameter seeding; an already-applied `bootstrap` profile therefore continues protecting ordinary reboots and OTA updates without rewriting saved configuration.

The password is not written to the V2 configuration record. Build every protected USB or OTA firmware with the same ignored local profile. For an `espota` upload, the library hook supplies PlatformIO's `--auth` flag from that same `device_password`; the local INI needs only the upload protocol and endpoint. Do not add a separate `--auth` option.

## Normal builds, migration, and reset

A normal build has no `custom_device_profile`. It generates no private header, starts with an empty device password, and never pre-seeds or overwrites saved configuration. Matching V2 data is loaded; a valid record for another application remains isolated and the new firmware uses defaults until its normal provisioning flow or an explicit profile provides values. Use this profile-free mode only for an intentionally open local device or interactive provisioning.

Adding, removing, or reordering parameter IDs requires no schema change. Bump the application schema only for a semantic conversion and supply a `DeviceFrameworkConfigMigrationCallback`; return `false` when a prior schema cannot be transformed safely. A firmware never loads a stored schema newer than itself.

**Reset Configuration** retains WiFi credentials and restores framework parameters to defaults. **Factory Reset** clears WiFi credentials and both transactional configuration slots. A V1 positional record is imported only when it exactly matches the bounded legacy layout, then rewritten as V2; that transitional reader will be removed in a future major release.
