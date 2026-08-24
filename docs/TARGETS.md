# Target organization

The supported release targets are `esp8266` (Wemos D1 mini) and `esp32` (ESP32 DevKit). Shared settings live in `[common]`; each target environment contains only board, platform, partition, and compiler differences.

To add another board family, add a separate `[env:<family>_<board>]` extending `[common]`, pin its PlatformIO platform version, and add a compile-only CI matrix entry. Do not copy dependencies or machine-specific paths into the new environment. Board-specific partitions and framework workarounds belong only in that environment.
