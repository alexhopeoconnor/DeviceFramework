# Changelog

## 2.7.0

- Add a precise configuration lifecycle contract: defaults, transactional storage, optional profiles, runtime edits, and schema migration each have one clear ownership boundary.
- Add revisioned reconcile-profile fixtures, direct migration/reconciliation coverage, and a real two-upload hardware regression on ESP8266 and ESP32.
- Defer MQTT and mDNS activity until a usable station connection exists, so a portal-only device does not block on broker resolution.
- Stream DFC4 payload decoding directly from EEPROM, removing the transient full-payload heap allocation; strengthen corrupted/foreign-record handling.
- Restore the shared `default1` development password after a factory reset, pin WiFiManager 3.2.1 and DFTE 1.2.0, and document organised local profile/test-input workflows.

## 2.6.0

- Add a platform-aware `DeviceFrameworkWebResourceLimits` policy, configured before setup, for bounded streamed responses, WebSerial capacity, memory headroom, and admission behavior.
- Serve built-in status and logo responses from fixed framework-owned slots, while bounding the remaining streamed page work instead of allowing an unbounded set of active responses.
- Replace WebSerial's dynamic client view and extraction `String` allocations with fixed client records and direct circular-buffer slices; overloads close with WebSocket code 1013 and diagnostic log data is dropped before device work is threatened.
- Refine existing web status polling and serial-tab lifecycle handling to avoid overlapping requests, stale error noise, and unnecessary diagnostic connections.
- Depend on DFTE 1.2.0 and document the measured resource policy, observability counters, configuration contract, and regression coverage.

## 2.5.1

- Correct explicit `reconcile` profiles so a newly applied profile revision updates its supplied shared device password alongside Wi-Fi and parameters.
- Preserve the active password when a reconcile profile omits `device_password`; bootstrap and corrupt-record recovery behaviour remains unchanged.
- Strengthen the connected-device regression to require a reconcile deployment to reject its superseded bootstrap password.

## 2.5.0

- Depend on WiFiManager 3.2.0 so the framework's provisioning tagline and updated portal contract resolve from published tags.
- Refine the existing web and provisioning experience with clearer status handling, safer connection hand-off, and consistent branded wording.
- Make DFC4 writes stream directly into the inactive EEPROM slot, then verify headers and CRCs before activation, reducing transient heap pressure and strengthening corruption recovery.
- Delay network services until Wi-Fi and mDNS have settled on ESP8266, and guard mDNS parsing on both total and contiguous heap headroom.
- Tighten format-2 profile validation, deployment tests, examples, and documentation for profile-free, bootstrap, and reconcile workflows.

### Upgrade note

- Rename `DeviceFrameworkBranding::provisioningIntro` to `provisioningTagline` when compiling a product branding override.

## 2.4.0

- Integrate ArduinoHA 3.1's automatic entity lifecycle: framework and sketch entities register without repeated `addDeviceType()` calls, while custom command handlers remain explicit.
- Hand broker connection ownership to `HAMqtt::loop()` after safe one-time initialization, and defer runtime broker reconfiguration until an inbound MQTT callback has completed.
- Pin ArduinoHA 3.1.0 and document the tested library stack.

## 2.3.0

- Add a bounded, framework-owned About section to `DeviceFrameworkUIConfig`: optional plain summary plus two validated static HTTPS links, rendered once at startup with escaped text and safe link attributes.
- Make DeviceFramework presentation neutral: replace the accidental Elixir default wording and opaque PNG with an editable source SVG plus deterministic Bash generation and validation tooling.
- Keep WiFiManager standalone-neutral while giving the Elixir firmware collection one shared consumer helper, canonical web logo, compact portal mark, and product About attribution.
- Extend compile and rendered-template regressions for the new fixed UI surface and generated assets.

## 2.2.2

- Correct the runtime library-version values so PlatformIO, Arduino IDE, and the package manifest all report 2.2.2.
- Add a deterministic version-bump command and release preflight checks for metadata, install references, and unfinished release notes.

## 2.2.1

- Pin the ArduinoHA package correction in v3.0.2 so PlatformIO and Arduino IDE consumers receive matching dependency metadata.

## 2.2.0

- Add an optional primary/fallback WiFi profile set, persisted in the transactional DFC4 record through WiFiManager 3.1.0. New portal profiles are committed only after a usable station connection.
- Treat prior DFC2 and DFC3 magic values as explicit profile-rebootstrap cases. Their payloads are not decoded; a normal profiled OTA can install DFC4 without a separate erase firmware.
- Add composable built-in web branding, correct streamed status JSON, and bounded runtime telemetry that distinguishes heap settling from a sustained leak trend.
- Pin Arduino-ESP32 3-compatible tooling and its dependent WiFi scan behaviour for reproducible ESP32 builds.

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
