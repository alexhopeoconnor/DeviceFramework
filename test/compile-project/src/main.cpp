#include <Arduino.h>
#include <DeviceFramework.h>

namespace {
#ifdef DEVICEFRAMEWORK_HARDWARE_SMOKE
constexpr const char* APPLICATION_ID = "deviceframework";
constexpr const char* FIRMWARE_VERSION = "0.0.0-hardware-smoke";
#else
constexpr const char* APPLICATION_ID = "deviceframework-compile-check";
constexpr const char* FIRMWARE_VERSION = "0.0.0-test";
#endif
constexpr uint16_t CONFIGURATION_SCHEMA = 1;

#ifndef DEVICEFRAMEWORK_TEST_DEFAULT_UI
DeviceFrameworkText text(const char* value) {
    return DeviceFrameworkText::ram(value);
}
#endif
}

void setup() {
#ifndef DEVICEFRAMEWORK_TEST_DEFAULT_UI
    DeviceFrameworkUIConfig ui;
#ifdef DEVICEFRAMEWORK_HARDWARE_SMOKE
    ui.branding.brandName = text("Test Lab");
    ui.branding.productName = text("DeviceFramework UI Test");
    ui.branding.provisioningTitle = text("Set up DeviceFramework UI Test");
    ui.branding.provisioningIntro = text("Connected-device portal and web UI verification.");
    ui.branding.logoAltText = text("Test Lab");
#else
    ui.branding.brandName = text("Compile Fixture");
    ui.branding.productName = text("DeviceFramework UI Check");
    ui.branding.provisioningTitle = text("Set up DeviceFramework UI Check");
    ui.branding.provisioningIntro = text("This verifies the shared portal and web UI configuration.");
    ui.branding.logoAltText = text("Compile Fixture");
#endif
    ui.about.summary = text("A fixed product attribution area in the existing web interface.");
    ui.about.primaryLink = {text("Example Devices"), text("https://example.test")};
    ui.about.creditLink = {text("Framework credits"), text("https://example.test/credits")};
    ui.theme.pageStart = text("#14532d");
    ui.theme.pageEnd = text("#166534");
    ui.theme.surface = text("#f0fdf4");
    ui.theme.text = text("#052e16");
    ui.theme.mutedText = text("#166534");
    ui.theme.border = text("#bbf7d0");
    ui.theme.accent = text("#15803d");
    ui.theme.accentHover = text("#166534");
    ui.theme.accentText = text("#ffffff");
    ui.theme.success = text("#16a34a");
    ui.theme.danger = text("#dc2626");
    ui.theme.cornerRadiusPx = 10;
    DeviceFramework::setUIConfig(ui);
#endif
    DeviceFramework::configureApplication(APPLICATION_ID, FIRMWARE_VERSION, CONFIGURATION_SCHEMA);
    DeviceFramework::beforeSetup();
#ifdef DEVICEFRAMEWORK_HARDWARE_SMOKE
    DeviceFramework::setup();
#endif
}

void loop() {
#ifdef DEVICEFRAMEWORK_HARDWARE_SMOKE
    DeviceFramework::loop();
#endif
}
