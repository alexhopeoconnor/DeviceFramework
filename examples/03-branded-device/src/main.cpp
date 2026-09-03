#include <Arduino.h>
#include <DeviceFramework.h>
#include "FirmwareIdentity.h"

namespace {
const char kBrand[] PROGMEM = "Example Devices";
const char kProduct[] PROGMEM = "Workshop Monitor";
const char kSetupTitle[] PROGMEM = "Set up Workshop Monitor";
const char kTagline[] PROGMEM = "A small connected-device example.";
const char kAccent[] PROGMEM = "#347a45";
const char kAccentText[] PROGMEM = "#ffffff";

void configurePresentation() {
    DeviceFrameworkUIConfig ui;
    ui.branding.brandName = DeviceFrameworkText::progmem(kBrand);
    ui.branding.productName = DeviceFrameworkText::progmem(kProduct);
    ui.branding.provisioningTitle = DeviceFrameworkText::progmem(kSetupTitle);
    ui.branding.provisioningTagline = DeviceFrameworkText::progmem(kTagline);
    ui.branding.logoAltText = DeviceFrameworkText::progmem(kBrand);
    ui.theme.accent = DeviceFrameworkText::progmem(kAccent);
    ui.theme.accentText = DeviceFrameworkText::progmem(kAccentText);
    ui.theme.cornerRadiusPx = 10;
    DeviceFramework::setUIConfig(ui);
}
}  // namespace

void setup() {
    configurePresentation();
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup([] {
        DeviceFramework::getParameterRegistry().setDefaultValue(
            DeviceFrameworkParameters::PARAM_DEVICE_NAME, "Workshop Monitor"
        );
    });
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
}
