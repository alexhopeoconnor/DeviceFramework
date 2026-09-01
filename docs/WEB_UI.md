# Web UI and provisioning branding

`DeviceFrameworkUIConfig` gives a firmware one setup-time product presentation configuration for two existing interfaces:

```text
DeviceFrameworkUIConfig
├── existing DeviceFramework web admin UI
└── private adapter → WiFiManagerPortalConfig → existing provisioning portal
```

DeviceFramework owns product identity, the existing web header, and web-theme tokens. WiFiManager owns provisioning routes, forms, navigation, captive behavior, and its portal API. DeviceFramework uses only WiFiManager public `setPortalConfig()`; WiFiManager never depends on DeviceFramework.

This is presentation-only source configuration. It creates no routes, pages, arbitrary HTML surfaces, stored fields, profile values, or schema migrations.

## Configure once before setup

UI values are non-owning static data in RAM or PROGMEM. Configure them before `beforeSetup()` and `setup()`:

```cpp
#include <DeviceFramework.h>

namespace {
const char kBrand[] PROGMEM = "Tree";
const char kProduct[] PROGMEM = "Temperature Monitor";
const char kSetupTitle[] PROGMEM = "Set up Temperature Monitor";
const char kIntro[] PROGMEM = "Connect this device to Wi-Fi.";
const char kPageStart[] PROGMEM = "#f4f7f3";
const char kPageEnd[] PROGMEM = "#dcebdd";
const char kSurface[] PROGMEM = "#ffffff";
const char kText[] PROGMEM = "#1c251e";
const char kAccent[] PROGMEM = "#347a45";
const char kAccentText[] PROGMEM = "#ffffff";
}

void setup() {
    DeviceFrameworkUIConfig ui;
    ui.branding.brandName = DeviceFrameworkText::progmem(kBrand);
    ui.branding.productName = DeviceFrameworkText::progmem(kProduct);
    ui.branding.provisioningTitle = DeviceFrameworkText::progmem(kSetupTitle);
    ui.branding.provisioningIntro = DeviceFrameworkText::progmem(kIntro);
    ui.branding.logoAltText = DeviceFrameworkText::progmem(kBrand);

    ui.theme.pageStart = DeviceFrameworkText::progmem(kPageStart);
    ui.theme.pageEnd = DeviceFrameworkText::progmem(kPageEnd);
    ui.theme.surface = DeviceFrameworkText::progmem(kSurface);
    ui.theme.text = DeviceFrameworkText::progmem(kText);
    ui.theme.accent = DeviceFrameworkText::progmem(kAccent);
    ui.theme.accentText = DeviceFrameworkText::progmem(kAccentText);
    ui.theme.cornerRadiusPx = 10;

    DeviceFramework::setUIConfig(ui);
    DeviceFramework::beforeSetup([] { /* register parameters */ });
    DeviceFramework::setup();
}
```

The small `ui` object may be local because the fields it refers to are static. The framework copies pointer-sized values, validates semantic theme values, prepares bounded escaped/style text before WiFi and web services start, and locks configuration before WiFiManager or web services start. `setUIConfig()` returns `false` after that point.

## What appears where

| `DeviceFrameworkUIConfig` value | Existing DeviceFramework web UI | Existing WiFiManager portal |
| --- | --- | --- |
| `brandName`, `productName`, `webTitle`, `logoAltText` | Header identity, document title (with `webTitle` as an optional override), and image label | Brand name becomes portal identity and fallback image label |
| `provisioningTitle`, `provisioningIntro` | — | Portal heading and home introduction |
| `portalLogoSvg` | — | Optional trusted inline SVG |
| `webLogo` | Optional base64 header image | — |
| `pageStart`, `pageEnd`, `surface`, `text`, `mutedText`, `border` | Existing admin UI surfaces | Shared values supported by WiFiManager |
| `accent`, `accentHover`, `accentText`, `success`, `danger`, `cornerRadiusPx` | Existing controls | Corresponding WiFiManager semantic tokens |

`productName` supplies the existing web heading and title unless `webTitle` is set. `webLogo.base64Data` is non-owning static base64 data. Mark a PROGMEM asset with `progmem = true`; the built-in DeviceFramework logo remains the default. DeviceFramework decodes it into the authenticated `/assets/deviceframework-logo` response instead of embedding it in every page. `portalLogoSvg` is a trusted compiled asset, never form, MQTT, or network input.

The generated web-theme block contains only CSS custom-property values. Existing status, parameter controls, restart behavior, WebSocket handling, endpoint structure, and firmware/version rendering remain unchanged.

## Firmware source versus local profiles

Keep UI values in committed firmware source. A local profile is deployment input only:

```text
FirmwareIdentity and FirmwareBranding  product identity, version, schema, UI
platformio.local.ini.<machine>         ignored profile and OTA selection
profiles.local/*.json                  ignored Wi-Fi, MQTT, password, device values
built firmware                         source UI plus selected deployment profile
```

UI values are not EEPROM configuration, so changing them needs no schema migration. USB and OTA deploy the exact same binary when built from the same source/profile selection.

For standalone portal fields, see the [WiFiManager portal UI guide](https://github.com/alexhopeoconnor/WiFiManager/blob/device-framework/docs/PORTAL_UI.md). For profiles, password rotation, and recovery behavior, see [Configuration and profiles](CONFIGURATION.md).

Back to the [documentation index](README.md) or [project overview](../README.md).

The base page links authenticated static `/assets/deviceframework.css` and `/assets/deviceframework.js` responses. They are streamed from PROGMEM and privately cached for five minutes, while the small per-device theme block remains in the HTML response. This keeps the dynamic page response small on ESP8266 without changing the user-facing UI.
