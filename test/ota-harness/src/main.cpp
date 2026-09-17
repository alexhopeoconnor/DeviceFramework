#include <Arduino.h>
#include <DeviceFramework.h>

#if !defined(DF_PORTAL_OTA_TEST) && !defined(DF_UDP_OTA_TEST)
#error "The OTA fixture must select either the portal or UDP transport contract."
#endif

#if defined(DF_PORTAL_OTA_TEST) && defined(DF_UDP_OTA_TEST)
#error "The OTA fixture cannot combine portal and UDP transport contracts."
#endif

#if defined(DF_PORTAL_OTA_TEST)
    #ifndef DF_PORTAL_OTA_IMAGE
        #error "The OTA portal fixture needs an immutable A or B image marker."
    #endif

    #ifndef DF_PORTAL_OTA_VERSION
        #error "The OTA portal fixture needs an immutable firmware version marker."
    #endif

    #ifndef DF_PORTAL_OTA_EXPECT_PROTECTED
        #error "The OTA portal fixture needs an explicit protected/open expectation."
    #endif

    #define DF_OTA_FIXTURE_IMAGE DF_PORTAL_OTA_IMAGE
    #define DF_OTA_FIXTURE_VERSION DF_PORTAL_OTA_VERSION
#endif

#if defined(DF_UDP_OTA_TEST)
    #ifndef DF_OTA_TEST_IMAGE
        #error "The UDP OTA fixture needs an immutable A or B image marker."
    #endif

    #ifndef DF_OTA_TEST_VERSION
        #error "The UDP OTA fixture needs an immutable firmware version marker."
    #endif

    #define DF_OTA_FIXTURE_IMAGE DF_OTA_TEST_IMAGE
    #define DF_OTA_FIXTURE_VERSION DF_OTA_TEST_VERSION
#endif

namespace {

constexpr const char* kApplicationId = "deviceframework";
constexpr uint16_t kConfigurationSchema = 1;
constexpr const char* kImageMarker = DF_OTA_FIXTURE_IMAGE;
constexpr const char* kFirmwareVersion = DF_OTA_FIXTURE_VERSION;

#ifdef DF_PORTAL_OTA_TEST
constexpr bool kExpectedPortalProtected = DF_PORTAL_OTA_EXPECT_PROTECTED != 0;

DeviceFrameworkText text(const char* value) {
    return DeviceFrameworkText::ram(value);
}

uint32_t otaCapacityBytes() {
#ifdef ESP8266
    // WiFiManager's ESP8266 HTTP updater reserves the same 4 KiB alignment
    // margin before it calls Update.begin(). Report that effective capacity so
    // the host refuses an oversized B image before invoking the browser.
    const uint32_t freeSketchSpace = ESP.getFreeSketchSpace();
    return freeSketchSpace > 0x1000UL
        ? ((freeSketchSpace - 0x1000UL) & 0xFFFFF000UL)
        : 0;
#else
    // On ESP32 this reports the next OTA partition selected by the running
    // framework. It is the same capacity Update.begin(UPDATE_SIZE_UNKNOWN)
    // will use, so the browser verifies the live board rather than a second
    // hard-coded copy of the partition-table size.
    return ESP.getFreeSketchSpace();
#endif
}

void registerPortalTestRoute() {
    // This route belongs only to the isolated test fixture. The callback runs
    // after WiFiManager owns the portal server and before it installs its own
    // routes, which is the supported test/host-integration hook.
    DeviceFramework::getWiFiManager().setWebServerCallback([]() {
        AsyncWebServer* server = DeviceFramework::getWiFiManager().getServer();
        if (server == nullptr) return;

        server->on("/api/test/firmware-marker", HTTP_GET,
            [](AsyncWebServerRequest* request) {
                const bool portalProtected = DeviceFramework::getDevicePassword()[0] != '\0';
                String body;
                body.reserve(160);
                body += F("{\"schema\":1,\"image\":\"");
                body += kImageMarker;
                body += F("\",\"version\":\"");
                body += kFirmwareVersion;
                body += F("\",\"portalProtected\":");
                body += portalProtected ? F("true") : F("false");
                body += F(",\"expectedPortalProtected\":");
                body += kExpectedPortalProtected ? F("true") : F("false");
                body += F(",\"otaCapacity\":");
                body += String(otaCapacityBytes());
                body += F("}");
                request->send(200, F("application/json"), body);
            });
    });
}

void configureFixtureUi() {
    DeviceFrameworkUIConfig ui;
    ui.branding.brandName = text("Test Lab");
    ui.branding.productName = text("DeviceFramework OTA Portal Test");
    ui.branding.provisioningTitle = text("Update DeviceFramework OTA Test");
    ui.branding.provisioningTagline = text("Portal firmware update verification.");
    ui.branding.logoAltText = text("Test Lab");
    ui.theme.pageStart = text("#eaf3ff");
    ui.theme.pageEnd = text("#d6e8ff");
    ui.theme.surface = text("#ffffff");
    ui.theme.text = text("#13233d");
    ui.theme.mutedText = text("#4b6385");
    ui.theme.border = text("#b9d4f4");
    ui.theme.accent = text("#2477c9");
    ui.theme.accentHover = text("#1d5f9f");
    ui.theme.accentText = text("#ffffff");
    ui.theme.success = text("#18864b");
    ui.theme.danger = text("#dc2626");
    ui.theme.cornerRadiusPx = 10;
    DeviceFramework::setUIConfig(ui);
}

#else

void configureFixtureUi() {
    // The UDP fixture exposes the normal DeviceFramework status endpoint, not
    // a portal route. Keep its setup intentionally small so the test proves
    // the station/ArduinoOTA path rather than presentation behaviour.
}

#endif

}  // namespace

void setup() {
    configureFixtureUi();
    DeviceFramework::configureApplication(
        kApplicationId, kFirmwareVersion, kConfigurationSchema);
#ifdef DF_PORTAL_OTA_TEST
    registerPortalTestRoute();
#endif

#ifdef DF_TEST_FORCE_MDNS_DEFERRED
    // Test the documented ESP8266 ownership model without manufacturing heap
    // fragmentation. ArduinoOTA's UDP listener must remain usable when the
    // framework deliberately declines to own an mDNS responder.
    setConfigMDNSMinFreeHeap(UINT32_MAX);
    setConfigMDNSMinLargestBlock(UINT32_MAX);
#endif

    DeviceFramework::beforeSetup();
    DeviceFramework::setup();

#ifdef DF_UDP_OTA_TEST
    // This appears in the serial capture alongside the framework's OTA and
    // mDNS lifecycle logs. The host still proves the immutable image identity
    // through a fresh authenticated /api/status response after each boot.
    Serial.print(F("DeviceFramework UDP OTA fixture image: "));
    Serial.println(kFirmwareVersion);
#endif
}

void loop() {
    DeviceFramework::loop();
}
