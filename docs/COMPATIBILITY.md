# Compatibility

DeviceFramework releases document the library stack in one place instead of duplicating a compatibility table in every dependency.

| DeviceFramework | WiFiManager | DFTE | home-assistant-integration | Platforms |
| --- | --- | --- | --- | --- |
| 1.x | 3.x | 1.x | 3.x | ESP8266, ESP32 |

Applications should pin the DeviceFramework release tag. Its manifest pins the maintained libraries to their tested release tags. The libraries use independent semantic versions: a breaking public API or behavior change increments the major version; backwards-compatible features increment minor; fixes increment patch.
