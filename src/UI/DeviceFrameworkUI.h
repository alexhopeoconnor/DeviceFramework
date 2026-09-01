#ifndef DEVICEFRAMEWORK_UI_H
#define DEVICEFRAMEWORK_UI_H

#include <Arduino.h>

enum class DeviceFrameworkUIStorage : uint8_t {
    Ram,
    Progmem,
};

/** Non-owning static text. The caller retains the data for the firmware lifetime. */
struct DeviceFrameworkText {
    const char* data = nullptr;
    DeviceFrameworkUIStorage storage = DeviceFrameworkUIStorage::Ram;

    static constexpr DeviceFrameworkText ram(const char* value) {
        return {value, DeviceFrameworkUIStorage::Ram};
    }
    static constexpr DeviceFrameworkText progmem(const char* value) {
        return {value, DeviceFrameworkUIStorage::Progmem};
    }

    bool empty() const { return data == nullptr || length() == 0; }
    size_t length() const {
        return data == nullptr ? 0 : (storage == DeviceFrameworkUIStorage::Progmem ? strlen_P(data) : strlen(data));
    }
    char at(size_t index) const {
        return storage == DeviceFrameworkUIStorage::Progmem
            ? static_cast<char>(pgm_read_byte(data + index))
            : data[index];
    }
};

/**
 * A base64 image for DeviceFramework's existing web-admin header.
 *
 * The framework does not take ownership of `base64Data`. Keep it static for
 * the firmware lifetime and identify PROGMEM data explicitly so it can be
 * streamed without a large RAM copy on ESP8266.
 */
struct DeviceFrameworkWebLogo {
    const char* base64Data = nullptr;
    const char* mimeType = "image/png";
    bool progmem = true;
};

/** Product identity displayed by DeviceFramework's existing web surfaces. */
struct DeviceFrameworkBranding {
    DeviceFrameworkText brandName;
    DeviceFrameworkText productName;  // Existing web heading and default document title.
    DeviceFrameworkText webTitle;     // Optional document-title override.
    DeviceFrameworkText provisioningTitle;
    DeviceFrameworkText provisioningIntro;
    DeviceFrameworkText portalLogoSvg;
    DeviceFrameworkText logoAltText;
    DeviceFrameworkWebLogo webLogo;
};

/** Semantic colour and shape values for DeviceFramework's web admin UI. */
struct DeviceFrameworkTheme {
    DeviceFrameworkText pageStart;
    DeviceFrameworkText pageEnd;
    DeviceFrameworkText surface;
    DeviceFrameworkText text;
    DeviceFrameworkText mutedText;
    DeviceFrameworkText border;
    DeviceFrameworkText accent;
    DeviceFrameworkText accentHover;
    DeviceFrameworkText accentText;
    DeviceFrameworkText success;
    DeviceFrameworkText danger;
    uint8_t cornerRadiusPx = 0;
};

/**
 * Setup-time UI configuration for a DeviceFramework firmware.
 *
 * It configures DeviceFramework's existing web interface directly and is
 * translated internally to WiFiManager's separate portal configuration.
 * It is intentionally not persisted with device configuration.
 */
struct DeviceFrameworkUIConfig {
    DeviceFrameworkBranding branding;
    DeviceFrameworkTheme theme;
};

class WiFiManager;

class DeviceFrameworkUI {
public:
    static bool setConfig(const DeviceFrameworkUIConfig& config);
    static const DeviceFrameworkUIConfig& getConfig();
    static bool isConfigured();

    /** Internal lifecycle hooks; applications use DeviceFramework::setUIConfig(). */
    static void applyPortalConfig(WiFiManager& wifiManager);
    static void lock();

    // Template accessors. The returned data remains valid for the firmware lifetime.
    static const char* getWebThemeStyle();
    static const DeviceFrameworkText& getBrandName();
    static const DeviceFrameworkText& getWebTitle();
    static const DeviceFrameworkText& getLogoAltText();
    static const char* getEscapedBrandName();
    static const char* getEscapedWebTitle();
    static const char* getEscapedLogoAltText();
    static const DeviceFrameworkWebLogo& getWebLogo();

private:
    static DeviceFrameworkUIConfig config;
    static DeviceFrameworkWebLogo defaultWebLogo;
    static DeviceFrameworkText defaultLogoAltText;
    static String webThemeStyle;
    static String escapedBrandName;
    static String escapedWebTitle;
    static String escapedLogoAltText;
    static bool configured;
    static bool locked;

    static bool isThemeValid(const DeviceFrameworkTheme& theme);
    static bool isTextValid(const DeviceFrameworkText& text);
    static void rebuildWebThemeStyle();
    static void rebuildEscapedText();
};

#endif  // DEVICEFRAMEWORK_UI_H
