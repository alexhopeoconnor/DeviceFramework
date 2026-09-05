# Web UI and provisioning branding

`DeviceFrameworkUIConfig` gives a firmware one setup-time product presentation configuration for two existing interfaces:

```text
DeviceFrameworkUIConfig
├── existing DeviceFramework web admin UI
└── private adapter → WiFiManagerPortalConfig → existing provisioning portal
```

DeviceFramework owns product identity, the existing web header, and web-theme tokens. WiFiManager owns provisioning routes, forms, navigation, captive behavior, and its portal API. DeviceFramework uses only WiFiManager public `setPortalConfig()`; WiFiManager never depends on DeviceFramework.

This is presentation-only source configuration. It creates no routes, consumer-defined pages, arbitrary HTML surfaces, stored fields, profile values, or schema migrations. A firmware may opt into one fixed framework-owned **About** section with a short summary and up to two static HTTPS links.

## Configure once before setup

UI values are non-owning static data in RAM or PROGMEM. Configure them before `beforeSetup()` and `setup()`:

```cpp
#include <DeviceFramework.h>

namespace {
const char kBrand[] PROGMEM = "Example Devices";
const char kProduct[] PROGMEM = "Temperature Monitor";
const char kSetupTitle[] PROGMEM = "Set up Temperature Monitor";
const char kTagline[] PROGMEM = "Connect this device to Wi-Fi.";
const char kPageStart[] PROGMEM = "#f4f7f3";
const char kPageEnd[] PROGMEM = "#dcebdd";
const char kSurface[] PROGMEM = "#ffffff";
const char kText[] PROGMEM = "#1c251e";
const char kAccent[] PROGMEM = "#347a45";
const char kAccentText[] PROGMEM = "#ffffff";
}

void configureUI() {
    DeviceFrameworkUIConfig ui;
    ui.branding.brandName = DeviceFrameworkText::progmem(kBrand);
    ui.branding.productName = DeviceFrameworkText::progmem(kProduct);
    ui.branding.provisioningTitle = DeviceFrameworkText::progmem(kSetupTitle);
    ui.branding.provisioningTagline = DeviceFrameworkText::progmem(kTagline);
    ui.branding.logoAltText = DeviceFrameworkText::progmem(kBrand);

    ui.theme.pageStart = DeviceFrameworkText::progmem(kPageStart);
    ui.theme.pageEnd = DeviceFrameworkText::progmem(kPageEnd);
    ui.theme.surface = DeviceFrameworkText::progmem(kSurface);
    ui.theme.text = DeviceFrameworkText::progmem(kText);
    ui.theme.accent = DeviceFrameworkText::progmem(kAccent);
    ui.theme.accentText = DeviceFrameworkText::progmem(kAccentText);
    ui.theme.cornerRadiusPx = 10;

    DeviceFramework::setUIConfig(ui);
}

void setup() {
    configureUI();
    DeviceFramework::beforeSetup([] { /* register parameters */ });
    DeviceFramework::setup();
}
```

The small `ui` object may be local **inside a short configuration helper** because the fields it refers to are static. The framework copies pointer-sized values, validates semantic theme values, prepares bounded escaped/style text before WiFi and web services start, and locks configuration before WiFiManager or web services start. On ESP8266, do not keep a large local UI configuration live across `DeviceFramework::setup()` in the same long-running `setup()` frame: the Arduino continuation stack is deliberately small. `setUIConfig()` returns `false` after setup has begun.

## What appears where

| `DeviceFrameworkUIConfig` value | Existing DeviceFramework web UI | Existing WiFiManager portal |
| --- | --- | --- |
| `brandName`, `productName`, `webTitle`, `logoAltText` | Header identity, document title (with `webTitle` as an optional override), and image label | Brand name becomes portal identity and fallback image label |
| `provisioningTitle`, `provisioningTagline` | — | Setup-page heading and short portal-header tagline |
| `portalLogoSvg` | — | Optional trusted inline SVG |
| `webLogo` | Optional base64 header image | — |
| `about.summary`, `about.primaryLink`, `about.creditLink` | Optional fixed About navigation and section | — |
| `pageStart`, `pageEnd`, `surface`, `text`, `mutedText`, `border` | Existing admin UI surfaces | Shared values supported by WiFiManager |
| `accent`, `accentHover`, `accentText`, `success`, `danger`, `cornerRadiusPx` | Existing controls | Corresponding WiFiManager semantic tokens |

`productName` supplies the existing web heading and title unless `webTitle` is set. `webLogo.base64Data` is non-owning static base64 data. Mark a PROGMEM asset with `progmem = true`; the built-in DeviceFramework logo remains the default. DeviceFramework decodes it into the authenticated `/assets/deviceframework-logo` response instead of embedding it in every page. `portalLogoSvg` is a trusted compiled asset, never form, MQTT, or network input.

The generated web-theme block contains only CSS custom-property values. Existing status, parameter controls, restart behavior, WebSocket handling, endpoint structure, and firmware/version rendering remain unchanged.

## Optional product About section

The framework owns the markup, navigation, spacing, and safe link attributes. A product only supplies plain static text and at most two complete label/HTTPS URL pairs:

```cpp
const char kAbout[] PROGMEM = "A compact connected controller for the workshop.";
const char kWebsiteLabel[] PROGMEM = "Example Devices";
const char kWebsiteUrl[] PROGMEM = "https://example.test";

ui.about.summary = DeviceFrameworkText::progmem(kAbout);
ui.about.primaryLink = {
    DeviceFrameworkText::progmem(kWebsiteLabel),
    DeviceFrameworkText::progmem(kWebsiteUrl),
};
```

Links are optional, but a label and URL must be provided together. URLs must use `https://` and are validated before setup; labels and summary are HTML-escaped once before WiFi and web services begin. The resulting anchors always use `target="_blank" rel="noopener noreferrer"`. This deliberately is not a custom-page or arbitrary-markup API.

## Built-in pages and browser work

The built-in web interface is server-rendered rather than a single-page application:

| Path | Purpose | Browser work |
| --- | --- | --- |
| `/` | Device status | Refreshes the status API only while this page is open. |
| `/serial` | WebSerial diagnostics | Opens WebSerial only after the page loads; leaving the page closes it. |
| `/controls` | Restart, reset, and device-password controls | No status polling or WebSerial connection. |
| `/about` | Firmware and product information | No status polling or WebSerial connection. |

This keeps normal status and control visits free of recurring diagnostic socket traffic. A bounded ESP8266 may return HTTP `503` when a second streamed page arrives before the first response is finished; that is the configured protection against fragmentation, not a restart. The existing [web resource limits](WEB_RESOURCES.md) API lets a consuming sketch select a higher response capacity after it has measured its own heap. When WebSerial is busy, the client receives the server's `1013` close and retries calmly after ten seconds rather than rapidly reconnecting.

## Editable default logo

`assets/deviceframework-mark.svg` is the neutral DeviceFramework source mark. `src/WebInterface/templates/WebInterfaceLogo.h` is generated from it for PROGMEM streaming. After changing the source asset, run `./tools/generate-web-assets.sh`; `./tools/check-web-assets.sh` verifies the checked-in generated header and is part of the normal framework validation commands.

## Firmware source versus local profiles

Keep UI values in committed firmware source. A local profile is deployment input only:

```text
FirmwareIdentity and FirmwareBranding  product identity, version, schema, UI
platformio.local.ini.<machine>         ignored profile and OTA selection
profiles.local/*.json                  ignored Wi-Fi, MQTT, password, device values
built firmware                         source UI plus selected deployment profile
```

UI values are not EEPROM configuration, so changing them needs no schema migration. USB and OTA deploy the exact same binary when built from the same source/profile selection.

The base page links authenticated static `/assets/deviceframework.css` and `/assets/deviceframework.js` responses. They are streamed from PROGMEM and privately cached for five minutes, while the small per-device theme block remains in the HTML response. This keeps the dynamic page response small on ESP8266 without changing the user-facing UI.

For standalone portal fields, see the [WiFiManager portal UI guide](https://github.com/alexhopeoconnor/WiFiManager/blob/device-framework/docs/PORTAL_UI.md). For profiles, password rotation, and recovery behavior, see [Configuration and profiles](CONFIGURATION.md).

Back to the [documentation index](README.md) or [project overview](../README.md).
