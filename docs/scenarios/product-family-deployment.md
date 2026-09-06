# Scenario: product-family deployment

## The real device pattern

Maintained consumers are separate firmware projects—temperature monitor,
solar/battery monitor, portable load, pressure/temperature switch, presence
controller, and several light controllers. Each pins the same released
DeviceFramework tag, chooses ESP8266 and/or ESP32 target settings, owns its
own identity/branding, and may select an ignored local deployment profile.

The result is shared lifecycle boilerplate without turning distinct products
into one monolithic sketch.

## Project layout

```text
product-firmware/
├── platformio.ini                     # tracked targets and release tag
├── include/FirmwareIdentity.h         # application ID, version, schema, branding
├── src/main.cpp                       # hardware and application policy
├── platformio.local.example.ini       # tracked selector example
├── profiles.example/bootstrap.json    # tracked safe template
└── profiles.local/site-a.json         # ignored deployment values
```

Pin `DeviceFramework` to a release tag in the tracked `platformio.ini`. Use
`lib_ldf_mode = deep+` for the framework's target-aware translation units.
Keep credentials, OTA endpoints, and per-site values in ignored local files;
do not make a consumer project depend on DeviceFramework's internal libraries
individually.

## Identity and source ownership

`APPLICATION_ID` identifies a persistent configuration family. Keep it stable
for normal releases of the same product, even if its display name changes.
`FIRMWARE_VERSION` tells operators and Home Assistant which product build is
running. Increase `CONFIGURATION_SCHEMA` only for a semantic transformation of
saved data.

Branding and UI policy belong in committed source adjacent to firmware identity,
because they describe the product binary. Deployment profile values are local
operator input: network, broker, password, and selected initial/reconciled
settings. The profile never becomes a second runtime authority.

## Deployment sequence

1. Build the tracked, profile-free project and confirm Portal First behaviour
   on each supported target.
2. Copy only safe templates, create ignored local profile/selector files, and
   validate the profile with a development board.
3. Bootstrap a new board with the selected profile or provision interactively.
4. Upgrade the firmware with the same application ID; verify existing operator
   changes survive ordinary boots and uploads.
5. Use a schema migration for renamed/reinterpreted values. Use a higher
   reconcile profile revision only for an explicit deployment rollout of named
   values.
6. Run target compile checks plus device-specific hardware/HA tests before
   production rollout.

## Test sequence

Test the product family against the real lifecycle, not just a clean flash:

- portal-first new device;
- profile bootstrap with and without Wi-Fi;
- a portal/HA edit that survives restart and firmware update;
- schema migration or reject path for an older stored value;
- both ESP8266 and ESP32 where advertised; and
- a Home Assistant broker/device restart for products that expose MQTT.

Related: [Getting started](../GETTING_STARTED.md) · [Configuration](../CONFIGURATION.md) · [Testing](../TESTING.md) · [Web UI branding](../WEB_UI.md).
