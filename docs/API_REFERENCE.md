# API reference

This reference covers the stable, sketch-facing DeviceFramework surfaces. Include
`<DeviceFramework.h>` for normal firmware. Most files under `src/` are library
implementation details even when PlatformIO can see them; do not bind product
code to their internal lifecycle or data layout.

## Support levels

| Level | Use | Examples |
| --- | --- | --- |
| Primary | Normal consuming sketches | `DeviceFramework`, parameters, UI setup, HA/MQTT accessors. |
| Advanced | Explicitly documented, platform-caveated behaviour | `DeviceFrameworkRtcBlob`, web resource policy, custom MQTT command topics. |
| Internal | Library implementation and generated web/storage plumbing | Private `src` modules, route handlers, templates, raw storage formats. |

## `DeviceFramework`

| API | When / purpose |
| --- | --- |
| `configureApplication(id, firmwareVersion, schema, migration?)` | Call once before setup. Establishes persistent application identity and optional semantic migration. |
| `getLibraryVersion()`, `getApplicationIdentity()` | Read release/application identity for diagnostics. |
| `beforeSetup(callback?)` | Initializes core setup state and invokes the one place to register custom parameters/defaults. Call before `setup()` when the sketch needs registration. |
| `setup()` | Loads saved configuration/profile input and initializes framework services. Call exactly once. |
| `loop()` | Service portal/network/mDNS/OTA/MQTT/web work every application loop. |
| `isInConfigMode()` | Read whether WiFiManager provisioning mode is active. |
| `getDeviceName()`, `getSanitizedHostname()` | Read current display name and safe host label. A changed name takes effect for startup transports after restart. |
| `getMqttServer()`, `getMqttPort()`, `getMqttUser()`, `getMqttPass()` | Read current broker configuration. |
| `setDeviceName()`, `setMqttServer()`, `setMqttPort()`, `setMqttUser()`, `setMqttPass()` | Direct in-memory core-parameter writes; call `saveParameters()` when the sketch owns persistence. |
| `getCustomParameterValue(id)`, `setCustomParameterValue(id, value)` | Compatibility helpers for custom registry values; prefer the registry for new code. |
| `getParameterRegistry()` | Canonical custom-parameter registration, access, validation, and change callback surface. |
| `getWiFiManager()` | Advanced access to the framework-owned WiFiManager; do not start a parallel portal/lifecycle. |
| `getHADevice()`, `getHAMqtt()` | Access framework-owned ArduinoHA device/client after setup. |
| `registerDeviceCommandHandler()`, `registerSharedCommandHandler()` | Register bounded custom MQTT command handlers before first loop. |
| `addMQTTResetCommand()`, `addMQTTRestartCommand()` | Add optional remote command topics after reviewing operational impact. |
| `setDevicePassword()`, `getDevicePassword()` | Read/transactionally rotate the shared local password; call framework `restart()` after a successful programmatic rotation. |
| `restart(reason)`, `getLastRestartReason()` | Restart with a one-shot framework provenance marker. On the next boot, the marker avoids rapid-reset recovery; `None` means no framework marker was present. |
| `reset(scope)`, `restoreDefaultParameters()` | Explicit recovery actions. See [Operations](OPERATIONS.md). |
| `saveParameters()`, `loadParameters()` | Explicit storage actions. Normal sketches rely on `setup()` to load and use `saveParameters()` only for direct programmatic writes. |

## Parameter registry

`DeviceFrameworkParameterRegistry` accepts `DeviceFrameworkParameterMetadata`.
Important members are:

| API | Purpose |
| --- | --- |
| `registerParameter(metadata)` | Add a custom parameter during `beforeSetup`. Returns false for invalid/duplicate metadata. |
| `setDefaultValue(id, value)` | Change a registered parameter's first-boot default before setup. |
| `getValue*` / `getValueAsInt/Float/Bool/CStr` | Read the current validated value. |
| `setValue(id, value, origin?)` | Validate, update runtime value, and sync supported surfaces. Direct calls do not save automatically. |
| `setChangeCallback(callback)` | Observe actual changes. Do not expect load-time callback replay; keep work non-blocking. |
| `hasParameter`, `getMetadata`, `getParameterIds*` | Inspect schema/registry for diagnostics or UI integration. |
| `getHADeviceForParameter(id)` | Advanced access to the framework-created HA entity when a sketch genuinely needs it. |

Read [Parameters](PARAMETERS.md) for metadata examples, persistence rules, and
the configuration-versus-state boundary.

## Home Assistant and MQTT

`DeviceFrameworkMQTT` provides static connection lifecycle helpers:

| API | Purpose |
| --- | --- |
| `onConnected(callback)`, `onDisconnected(callback)` | Receive quick, repeatable broker transition notifications. |
| `isConnected()` | Read current broker state without blocking. |
| `getHADevice()`, `getHAMqtt()` | Same objects as `DeviceFramework` accessors. |
| `generateDeviceSpecificTopic()`, `generateSharedTopic()` | Inspect framework-generated custom command topics; normally register a handler rather than publishing raw copies of HA semantics. |

Native ArduinoHA entities are configured by ArduinoHA's API and must outlive
setup. DeviceFramework registers them automatically. See
[Home Assistant and MQTT](HOME_ASSISTANT_MQTT.md).

## Presentation and optional web service

| API | Availability / use |
| --- | --- |
| `DeviceFramework::setUIConfig(config)` | Before setup only. Source-owned branding/theme for existing web and provisioning surfaces. |
| `DeviceFramework::getUIConfig()` | Read current presentation configuration. |
| `DeviceFrameworkWeb::defaultResourceLimits()` | With `ENABLE_WEB_INTERFACE`; returns platform-aware default policy. |
| `DeviceFrameworkWeb::setResourceLimits(limits)` | With `ENABLE_WEB_INTERFACE`; before web setup only. |
| `DeviceFrameworkWeb::getResourceStats()` | With `ENABLE_WEB_INTERFACE`; retrieve diagnostic counters, not a versioned HTTP status schema. |
| `setupWebInterface()`, `shutdownWebInterface()`, `restartWebInterface()`, `webInterfaceLoop()` | Legacy/manual web controls. Normal sketches let `setup()`/`loop()` own the web service. |

Read [Web UI branding](WEB_UI.md) and [Web resource limits](WEB_RESOURCES.md)
before changing these policies.

## Advanced restart-resilient blob

`DeviceFrameworkRtcBlob` is a small, CRC-protected storage surface for an
application's compact restart-resilient state:

```cpp
struct RelayIntent { uint8_t requestedOn; };
RelayIntent intent{1};
DeviceFrameworkRtcBlob::write("example.relay_intent", &intent, sizeof(intent));
```

`write(name, data, len)`, `read(name, data, capacity, outLen?)`, and
`clear(name)` accept up to 64 bytes. It is an advanced helper, not transactional
configuration storage: on ESP8266 there is one shared RTC slot, so the most
recent blob write wins even if names differ. ESP32 stores entries in its
Preferences namespace. Always validate application semantics after reading and
always establish electrically safe hardware before acting on a recovered
intent.

Next: [Lifecycle](LIFECYCLE.md) · [Parameters](PARAMETERS.md) · [Operations](OPERATIONS.md).
