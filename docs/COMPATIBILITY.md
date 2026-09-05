# Compatibility

DeviceFramework releases document the library stack in one place instead of duplicating a compatibility table in every dependency.

| DeviceFramework | WiFiManager | DFTE | home-assistant-integration | Platforms |
| --- | --- | --- | --- | --- |
| 2.7.x | 3.2.1 | 1.2.0 | 3.1.0 | ESP8266, ESP32 |
| 2.6.x | 3.2.0 | 1.2.0 | 3.1.0 | ESP8266, ESP32 |
| 2.5.x | 3.2.0 | 1.1.0 | 3.1.0 | ESP8266, ESP32 |
| 2.4.x | 3.1.0 | 1.1.0 | 3.1.0 | ESP8266, ESP32 |
| 2.3.x | 3.1.0 | 1.1.0 | 3.0.2 | ESP8266, ESP32 |
| 2.2.x | 3.1.0 | 1.0.2 | 3.0.0 | ESP8266, ESP32 |
| 2.1.x | 3.0.6 | 1.0.2 | 3.0.0 | ESP8266, ESP32 |
| 2.0.x | 3.0.5 | 1.0.2 | 3.0.0 | ESP8266, ESP32 |

Applications should pin the DeviceFramework release tag. Its manifest pins the maintained libraries to their tested release tags. The libraries use independent semantic versions: a breaking public API or behavior change increments the major version; backwards-compatible features increment minor; fixes increment patch.

Back to [documentation](README.md) · [project overview](../README.md).
