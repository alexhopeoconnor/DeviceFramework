#ifdef ENABLE_WEB_INTERFACE
// Include DeviceFrameworkConfig.h first to ensure config defaults are available
// when template headers are processed via DeviceFrameworkTemplatePlaceholders.h
#include "../Configuration/DeviceFrameworkConfig.h"
#include "DeviceFrameworkTemplatePlaceholders.h"
#include "templates/WebInterfaceFavicon.h"
#include "templates/WebInterfaceHTML.h"
#include "../DeviceFrameworkDebug.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../UI/DeviceFrameworkUI.h"

// Static member definitions
PlaceholderRegistry* DeviceFrameworkTemplatePlaceholders::registry = nullptr;
bool DeviceFrameworkTemplatePlaceholders::isSetup = false;

namespace {

String htmlEscape(const char* value) {
    String escaped;
    if (value == nullptr) return escaped;
    for (const char* p = value; *p; ++p) {
        switch (*p) {
            case '&': escaped += F("&amp;"); break;
            case '<': escaped += F("&lt;"); break;
            case '>': escaped += F("&gt;"); break;
            case '"': escaped += F("&quot;"); break;
            case '\'': escaped += F("&#39;"); break;
            default: escaped += *p; break;
        }
    }
    return escaped;
}

}  // namespace

// Getter functions for dynamic data
const char* DeviceFrameworkTemplatePlaceholders::getPageTitle() {
    static String title;
    const DeviceFrameworkText& configuredTitle = DeviceFrameworkUI::getWebTitle();
    if (!configuredTitle.empty()) {
        return DeviceFrameworkUI::getEscapedWebTitle();
    }

    String deviceName = DeviceFrameworkParameters::getDeviceName();
    const String rawTitle = deviceName.length() > 0
        ? deviceName + " - Control Panel"
        : String("DeviceFramework Control Panel");
    title = htmlEscape(rawTitle.c_str());
    return title.c_str();
}

const char* DeviceFrameworkTemplatePlaceholders::getPageTitle404() {
    static String title;
    const DeviceFrameworkText& configuredTitle = DeviceFrameworkUI::getWebTitle();
    if (!configuredTitle.empty()) {
        title = String(DeviceFrameworkUI::getEscapedWebTitle()) + F(" - 404 Not Found");
        return title.c_str();
    }

    String deviceName = DeviceFrameworkParameters::getDeviceName();
    const String rawTitle = deviceName.length() > 0
        ? deviceName + " - 404 Not Found"
        : String("404 - Page Not Found");
    title = htmlEscape(rawTitle.c_str());
    return title.c_str();
}

const char* DeviceFrameworkTemplatePlaceholders::getUiTheme() {
    return DeviceFrameworkUI::getWebThemeStyle();
}

const char* DeviceFrameworkTemplatePlaceholders::getBrandName() {
    return DeviceFrameworkUI::getEscapedBrandName();
}

const char* DeviceFrameworkTemplatePlaceholders::getLogoAltText() {
    return DeviceFrameworkUI::getEscapedLogoAltText();
}

const char* DeviceFrameworkTemplatePlaceholders::getAboutNavigation() {
    return DeviceFrameworkUI::getAboutNavigation();
}

const char* DeviceFrameworkTemplatePlaceholders::getAboutSection() {
    return DeviceFrameworkUI::getAboutSection();
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

    // Stream large assets directly from their declared storage. A product logo
    // is optional; the library-owned PROGMEM logo remains the default.
    registry->registerProgmemData("%FAVICON_BASE64%", (const char*)favicon_base64);

    // Only page-specific dynamic values are rendered through getters; the
    // large stylesheet and script are served separately from PROGMEM.
    registry->registerRamData("%PAGE_TITLE%", getPageTitle);
    registry->registerRamData("%PAGE_TITLE_404%", getPageTitle404);
    registry->registerRamData("%UI_THEME%", getUiTheme);
    registry->registerRamData("%BRAND_NAME%", getBrandName);
    registry->registerRamData("%LOGO_ALT_TEXT%", getLogoAltText);
    registry->registerRamData("%ABOUT_NAV%", getAboutNavigation);
    registry->registerRamData("%ABOUT_SECTION%", getAboutSection);

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
