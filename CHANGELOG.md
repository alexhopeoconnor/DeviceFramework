# Changelog

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
