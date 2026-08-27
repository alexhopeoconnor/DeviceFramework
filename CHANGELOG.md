# Changelog

## 2.2.0

- Add an optional primary/fallback WiFi profile set, persisted in the transactional DFC4 record through WiFiManager 3.1.0. New portal profiles are committed only after a usable station connection.
- Treat prior DFC2 and DFC3 magic values as explicit profile-rebootstrap cases. Their payloads are not decoded; a normal profiled OTA can install DFC4 without a separate erase firmware.

## 2.1.0

- Replace V2/V1 storage readers with V3 transactional records that bind the active shared device password to the same CRC-validated configuration write as parameters and provisioning state.
- Make local profiles initial seeds and recovery fallbacks only. A valid V3 password now survives ordinary boots and later profiled OTA firmware; runtime rotation is available through `DeviceFramework::setDevicePassword()` and the optional web interface.
- Classify DFC2 without decoding it, allowing an explicitly selected profile to replace it in one normal boot while avoiding legacy mappers and erase-only update flows.
- Remove obsolete AdminPassword compatibility aliases and redundant web-loop sketch boilerplate; document V3 reset, profile, password-rotation, and OTA behavior.

## 2.0.1

- Treat a fully validated V2 record from another application as an explicit
  foreign-application state, so a local bootstrap profile can safely provision
  a new firmware family without an erase while preserving corruption handling.

- Restore a selected profile's shared device password on every boot, even when
  `bootstrap` correctly leaves matching stored WiFi and parameters untouched.
- Make that profile password the sole `espota` authentication source and add a
  safe profiled regression fixture for ESP8266 and ESP32.
- Correct the PlatformIO dependency metadata, pin WiFiManager 3.0.5, and add
  board-free clean-consumer compile checks that declare only DeviceFramework.

## 2.0.0

- Replace positional `V1.0` EEPROM records with application-bound, ID-keyed V2 records in two CRC-validated slots.
- Separate framework release version, firmware version, storage format, and application configuration schema.
- Add application migration callbacks, bounded V1 import, explicit configuration/factory reset scopes, and dynamic parameter capacity.
- Add optional private PlatformIO JSON profiles for one-time bootstrap or revisioned reconciliation of WiFi, device password, and parameters.
- Validate numeric, enum, and boolean values before persistence; core MQTT port and log level now declare their constraints.

### Upgrade notes

Existing V1 records are imported once only when their legacy layout is recognised, then rewritten as V2. Applications must call `DeviceFramework::configureApplication()` before `beforeSetup()`; the maintained sketches do this through `FirmwareIdentity.h`.

## 1.0.0

- First standalone public release of DeviceFramework.
- Add reproducible Git-tagged dependencies, portable PlatformIO configuration, ignored local overrides, and board-free compile checks.
- Use one optional validated device password for provisioning, OTA, HTTP Basic authentication, and WebSerial.
- Correct parameter-ID ownership, EEPROM version reads, WebSerial full-buffer handling, and web-disabled logging builds.
- Redact password-type parameter values from framework persistence and synchronization logs.
