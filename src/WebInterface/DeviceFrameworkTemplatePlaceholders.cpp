#ifdef ENABLE_WEB_INTERFACE
// Include DeviceFrameworkConfig.h first to ensure config defaults are available
// when template headers are processed via DeviceFrameworkTemplatePlaceholders.h
#include "../Configuration/DeviceFrameworkConfig.h"
#include "DeviceFrameworkTemplatePlaceholders.h"
#include "templates/WebInterfaceFavicon.h"
#include "templates/WebInterfaceLogo.h"
#include "templates/WebInterfaceCSS.h"
#include "templates/WebInterfaceJS.h"
#include "templates/WebInterfaceHTML.h"
#include "../DeviceFrameworkDebug.h"
#include "../Configuration/DeviceFrameworkParameters.h"

// Static member definitions
PlaceholderRegistry* DeviceFrameworkTemplatePlaceholders::registry = nullptr;
bool DeviceFrameworkTemplatePlaceholders::isSetup = false;

// Getter functions for dynamic data
const char* DeviceFrameworkTemplatePlaceholders::getPageTitle() {
    static String title;
    String deviceName = DeviceFrameworkParameters::getDeviceName();
    if (deviceName.length() > 0) {
        title = deviceName + " - Control Panel";
    } else {
        title = "DeviceFramework Control Panel";
    }
    return title.c_str();
}

const char* DeviceFrameworkTemplatePlaceholders::getPageTitle404() {
    static String title;
    String deviceName = DeviceFrameworkParameters::getDeviceName();
    if (deviceName.length() > 0) {
        title = deviceName + " - 404 Not Found";
    } else {
        title = "404 - Page Not Found";
    }
    return title.c_str();
}

void DeviceFrameworkTemplatePlaceholders::setup() {
    if (isSetup) {
        LOG_DEBUGLN(F("Template placeholders already initialized"));
        return;
    }

    LOG_INFOLN(F("Setting up DeviceFramework template placeholders..."));

    // Create registry using config value (allows creating with custom size if needed)
    // Defaults to CONFIG_maxTemplatePlaceholders from DeviceFrameworkConfig
    registry = new PlaceholderRegistry(getConfigMaxTemplatePlaceholders());

    // Register all placeholders
    registerAllPlaceholders();

    isSetup = true;
    LOG_INFO_SP(F("Registered "), true);
    LOG_INFO_SP(String(registry->getCount()), false);
    LOG_INFOLN_SP(F(" template placeholders"), false);
}

void DeviceFrameworkTemplatePlaceholders::registerAllPlaceholders() {
    if (!registry) return;

    registry->clear();

    // Register PROGMEM data (large assets)
    registry->registerProgmemData("%FAVICON_BASE64%", (const char*)favicon_base64);
    registry->registerProgmemData("%STYLES%", (const char*)css_styles);
    registry->registerProgmemData("%LOGO_BASE64%", (const char*)logo_base64);
    registry->registerProgmemData("%SCRIPTS%", (const char*)js_scripts);

    // Register dynamic data (RAM with getters)
    registry->registerRamData("%PAGE_TITLE%", getPageTitle);
    registry->registerRamData("%PAGE_TITLE_404%", getPageTitle404);

    // Register PROGMEM templates (nested templates)
    registry->registerProgmemTemplate("%HEADER%", (const char*)header_template);
    registry->registerProgmemTemplate("%HEADER_404%", (const char*)header_404_template);
    registry->registerProgmemTemplate("%NAV%", (const char*)nav_template);
    registry->registerProgmemTemplate("%FOOTER%", (const char*)footer_template);
    registry->registerProgmemTemplate("%SPA_CONTENT%", (const char*)spa_content_template);
    registry->registerProgmemTemplate("%404_CONTENT%", (const char*)error404_content_template);
    registry->registerProgmemTemplate("%CONTENT%", (const char*)spa_content_template);
}

void DeviceFrameworkTemplatePlaceholders::cleanup() {
    if (!isSetup) {
        return;
    }

    LOG_DEBUGLN(F("Cleaning up template placeholders..."));

    if (registry) {
        delete registry;
        registry = nullptr;
    }

    isSetup = false;
    LOG_INFOLN(F("Template placeholders cleaned up"));
}

PlaceholderRegistry* DeviceFrameworkTemplatePlaceholders::getRegistry() {
    return registry;  // May return nullptr if not initialized
}

#endif // ENABLE_WEB_INTERFACE
