# Application configuration and local profiles

Each consuming firmware declares three separate values in `include/FirmwareIdentity.h`:

- `APPLICATION_ID` identifies the persistent configuration family. Keep it stable for ordinary firmware releases.
- `FIRMWARE_VERSION` is reported by the running firmware and Home Assistant.
- `CONFIGURATION_SCHEMA` changes only when the meaning of an existing saved value changes.

Adding, removing, or reordering parameter IDs does not need a schema bump: V4 storage is keyed by parameter ID. Increase the schema only for a rename, conversion, clamp, or another incompatible interpretation.

## Optional local profile

A profile is an opt-in build input owned by the consuming firmware. Keep its safe template beside that firmware and its actual device values in an ignored sibling directory:

```text
your-firmware/
├── platformio.ini                    # tracked portable build
├── platformio.local.example.ini      # tracked local selector example
├── profiles.example/bootstrap.json   # tracked safe template
└── profiles.local/home.json          # ignored real profile
```

The ignored `platformio.local.ini.<machine>` only selects a local profile or OTA endpoint; real values remain in the JSON profile. A consuming project may point DeviceFramework at a local checkout, but PlatformIO still resolves that checkout's manifest Git refs, so coordinated untagged dependency work belongs in the framework root project. DeviceFramework's package hook uses PlatformIO/SCons and Python’s standard library to generate a C++ header only under `.pio`. Sketches do not declare `extra_scripts`, manage Python, or parse credentials. With no `custom_device_profile`, the hook does nothing.

## Profile contract

The compiler requires `format: 2` and rejects unknown keys at every profile level. Every consumed string must contain printable ASCII. `application` and `profile.id` must be non-empty; `profile.revision` is an integer from 1 through 4,294,967,295; and `profile.policy` is either `bootstrap` or `reconcile`. `wifi` is optional. When present, `wifi.profiles` contains one primary profile and at most one fallback; SSIDs are 1–32 characters and Wi-Fi passwords are 0–64 characters. `parameters` is optional, but its IDs are alphanumeric and its values use the same printable-ASCII rule. `device_password` is optional; when supplied, it must be empty or 8–31 characters.

```json
{
  "format": 2,
  "application": "your-firmware",
  "profile": { "id": "home-temperature", "revision": 1, "policy": "bootstrap" },
  "device_password": "8-to-31-character-password",
  "wifi": {
    "profiles": [
      { "ssid": "network", "password": "network-password" },
      { "ssid": "fallback-network", "password": "fallback-password" }
    ]
  },
  "parameters": { "device": "Workshop monitor", "mqttserver": "mqtt.local" }
}
```

A profile that supplies `wifi.profiles` needs one primary WiFi object; the second object is an optional fallback. The controller verifies a new candidate before it persists the profile set, then prefers the last successful network on later boots. Omit the entire `wifi` object when a profile should seed only non-network settings and intentionally open the interactive WiFiManager portal:

```json
{
  "format": 2,
  "application": "your-firmware",
  "profile": { "id": "portal-test", "revision": 1, "policy": "bootstrap" },
  "parameters": { "device": "Workshop monitor" }
}
```

A profile overrides only the parameter IDs it names. On a new or reset device, omitted core MQTT settings retain the portable defaults: broker `mqtt.local`, port `1883`, and empty username/password. Put real broker credentials only in the ignored local profile (or enter them through provisioning); never add them to the framework defaults or a tracked example.

`bootstrap` is the safe default for a new device. It applies to empty storage, a valid record belonging to another `APPLICATION_ID`, or recognised DFC2 or DFC3 storage; it does not overwrite matching-application configuration or corrupt/unverified V4 data. This allows explicit erase-free handover between applications or from recognised 2.0.x storage.

## Upgrade from DeviceFramework 2.1.x

DeviceFramework 2.1.x records are DFC3. They are deliberately not decoded by 2.2.x. Select the same ignored local deployment profile, including a primary WiFi candidate, and perform one normal authenticated USB or OTA update: the profile seeds a DFC4 record, verifies its primary/fallback WiFi candidate, and then stores the verified profiles. There is no special erase firmware. A profile-free 2.2.x build treats DFC3 as unsupported and opens provisioning instead of guessing at an incompatible layout.

`reconcile` deliberately applies supplied managed values once per profile ID/revision and records the attempt before Wi-Fi connects. Increase `profile.revision` after changing a reconciliation profile. Use it only when profile values are intended to take precedence over existing portal-managed values.

## One shared device password

`device_password` is optional. A `bootstrap` profile seeds it only for a new or recovery configuration. A `reconcile` profile that explicitly supplies it writes that value once when its profile ID or revision changes, alongside its other managed values. Omit the field from a reconcile profile to leave the existing password untouched; explicitly supply `""` to clear it. In every case, the saved value restores on ordinary boots and OTA updates without the profile becoming a second runtime authority.

Use the public API to rotate it at runtime:

```cpp
if (!DeviceFramework::setDevicePassword("new-local-password")) {
    // Invalid value or verified storage write failed; the old password remains active.
}
```

The optional web interface provides the same operation at **System Controls → Device Password**. The one active value protects the provisioning AP, Arduino OTA, HTTP Basic authentication, and WebSerial. For OTA, the build-time profile supplies `espota --auth`; after runtime rotation, update that ignored JSON value before the next OTA upload. Matching the JSON only authenticates the uploader—the verified V4 record remains the device’s runtime source of truth.

## ESP8266 mDNS heap guard

The ESP8266 core allocates from its Wi-Fi system context while probing and parsing multicast mDNS traffic. DeviceFramework therefore starts the responder, calls its parser, and drains resolver packets only when it has both 4 KB free heap and a 4 KB contiguous heap block; this avoids an allocator reset when otherwise-adequate free heap is fragmented. A numeric endpoint such as an MQTT broker IP bypasses mDNS resolution while the responder is deferred. The guard is sampled at most every 25 ms, rather than every application-loop iteration, because the ESP8266 heap-stat query itself scans allocator state. These are conservative defaults. Leave them unchanged unless measurement on the target device shows a different trade-off is required.

```cpp
setConfigMDNSMinFreeHeap(5120);      // Require 5 KB total free heap before mDNS work.
setConfigMDNSMinLargestBlock(5120);  // Require a 5 KB contiguous block before mDNS work.
setConfigMDNSUpdateInterval(50);     // Check and process mDNS at most 20 times per second.
```

`DeviceFrameworkMDNS::getUpdateCount()` and
`DeviceFrameworkMDNS::getUpdateSkippedForHeapCount()` expose lightweight uptime
diagnostics without logging every loop. A rising skip count during a memory-heavy
operation is expected; it should stop rising once the contiguous heap recovers.

## Normal builds, migration, and reset

A normal build has no `custom_device_profile`: it never generates private source, seeds, or overwrites stored configuration. It loads a matching V4 record, including its password, or follows normal WiFiManager provisioning. A selected profile that omits `wifi` behaves the same way for networking while still seeding its permitted password and parameters. A valid foreign application record remains isolated. Recognised DFC2 or DFC3 is never silently decoded or erased; an explicit profile can replace it.

Use a migration callback only after increasing `CONFIGURATION_SCHEMA` for a semantic change:

```cpp
inline bool migrate(uint16_t fromSchema, DeviceFrameworkConfigMigration& values) {
    if (fromSchema != 1) return false;
    return values.rename("sampleperiod", "sampleinterval") &&
           values.multiplyUInt("sampleinterval", 60);
}

inline bool configure() {
    return DeviceFramework::configureApplication(
        APPLICATION_ID, FIRMWARE_VERSION, 2, migrate
    );
}
```

Return `false` when a previous schema cannot be transformed safely. Firmware never loads a saved schema newer than it understands.

**Reset Configuration** keeps Wi-Fi credentials and the active device password while restoring framework parameters. **Factory Reset** clears Wi-Fi, password, and both transactional slots. Current releases write DFC4, introduced in DeviceFramework 2.2.0. DFC2 and DFC3 are recognised only as unsupported markers so a selected profile can replace them; their values are not decoded or migrated.

Next: [testing](TESTING.md) · [development and releases](DEVELOPMENT.md) · [documentation map](README.md).
