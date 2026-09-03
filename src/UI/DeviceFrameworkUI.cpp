#include "DeviceFrameworkUI.h"

#include <WiFiManager.h>

#ifdef ENABLE_WEB_INTERFACE
#include "../WebInterface/templates/WebInterfaceLogo.h"
#endif

namespace {

bool isSafeCssValue(const DeviceFrameworkText& value) {
    const size_t length = value.length();
    if (length > 64) return false;
    for (size_t i = 0; i < length; i++) {
        const char c = value.at(i);
        if (!(isAlphaNumeric(c) || c == '#' || c == '(' || c == ')' || c == ',' ||
              c == '.' || c == '%' || c == ' ' || c == '-' || c == '/')) {
            return false;
        }
    }
    return true;
}

void appendCssToken(String& stylesheet, const char* name, const DeviceFrameworkText& value) {
    if (value.empty()) return;
    stylesheet += name;
    stylesheet += ':';
    for (size_t i = 0; i < value.length(); ++i) stylesheet += value.at(i);
    stylesheet += ';';
}

size_t escapedTextLength(const DeviceFrameworkText& value) {
    size_t length = 0;
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value.at(i)) {
            case '&': length += 5; break;
            case '<':
            case '>': length += 4; break;
            case '"': length += 6; break;
            case 39: length += 5; break;
            default: ++length; break;
        }
    }
    return length;
}

void appendHtmlEscaped(String& destination, const DeviceFrameworkText& value) {
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value.at(i)) {
            case '&': destination += F("&amp;"); break;
            case '<': destination += F("&lt;"); break;
            case '>': destination += F("&gt;"); break;
            case '"': destination += F("&quot;"); break;
            case 39: destination += F("&#39;"); break;
            default: destination += value.at(i); break;
        }
    }
}

WiFiManagerPortalText mapPortalText(const DeviceFrameworkText& value) {
    return {value.data, value.storage == DeviceFrameworkUIStorage::Progmem
        ? WiFiManagerPortalStorage::Progmem
        : WiFiManagerPortalStorage::Ram};
}

const char kDefaultLogoAltText[] PROGMEM = "DeviceFramework";

}  // namespace

DeviceFrameworkUIConfig DeviceFrameworkUI::config;
#ifdef ENABLE_WEB_INTERFACE
DeviceFrameworkWebLogo DeviceFrameworkUI::defaultWebLogo = {logo_base64, "image/svg+xml", true};
#else
DeviceFrameworkWebLogo DeviceFrameworkUI::defaultWebLogo;
#endif
DeviceFrameworkText DeviceFrameworkUI::defaultLogoAltText = DeviceFrameworkText::progmem(kDefaultLogoAltText);
String DeviceFrameworkUI::webThemeStyle;
String DeviceFrameworkUI::escapedBrandName;
String DeviceFrameworkUI::escapedWebTitle;
String DeviceFrameworkUI::escapedLogoAltText;
String DeviceFrameworkUI::aboutNavigation;
String DeviceFrameworkUI::aboutSection;
bool DeviceFrameworkUI::configured = false;
bool DeviceFrameworkUI::locked = false;

bool DeviceFrameworkUI::isThemeValid(const DeviceFrameworkTheme& theme) {
    return isSafeCssValue(theme.pageStart) && isSafeCssValue(theme.pageEnd) &&
        isSafeCssValue(theme.surface) && isSafeCssValue(theme.text) &&
        isSafeCssValue(theme.mutedText) && isSafeCssValue(theme.border) &&
        isSafeCssValue(theme.accent) && isSafeCssValue(theme.accentHover) &&
        isSafeCssValue(theme.accentText) && isSafeCssValue(theme.success) &&
        isSafeCssValue(theme.danger) && theme.cornerRadiusPx <= 64;
}

bool DeviceFrameworkUI::isTextValid(const DeviceFrameworkText& text) {
    if (text.length() > 256) return false;
    for (size_t i = 0; i < text.length(); ++i) {
        const char c = text.at(i);
        if (c < 32 || c > 126) return false;
    }
    return true;
}

bool DeviceFrameworkUI::isExternalLinkValid(const DeviceFrameworkExternalLink& link) {
    if (!isTextValid(link.label) || !isTextValid(link.url)) return false;
    const bool hasLabel = !link.label.empty();
    const bool hasUrl = !link.url.empty();
    if (hasLabel != hasUrl) return false;
    if (!hasUrl) return true;

    constexpr char kHttpsPrefix[] = "https://";
    if (link.url.length() > 192 || link.url.length() <= sizeof(kHttpsPrefix) - 1) return false;
    for (size_t index = 0; index < sizeof(kHttpsPrefix) - 1; ++index) {
        if (link.url.at(index) != kHttpsPrefix[index]) return false;
    }
    if (link.url.at(sizeof(kHttpsPrefix) - 1) == '/' ||
        link.url.at(sizeof(kHttpsPrefix) - 1) == '?' ||
        link.url.at(sizeof(kHttpsPrefix) - 1) == '#') return false;

    // Permit the ASCII URL subset used by normal HTTPS web links, rejecting
    // whitespace and HTML-attribute characters before templates are rendered.
    for (size_t index = sizeof(kHttpsPrefix) - 1; index < link.url.length(); ++index) {
        const char c = link.url.at(index);
        if (!(isAlphaNumeric(c) || c == '-' || c == '.' || c == '_' || c == '~' ||
              c == ':' || c == '/' || c == '?' || c == '#' || c == '[' || c == ']' ||
              c == '@' || c == '!' || c == '$' || c == '&' || c == 39 || c == '(' ||
              c == ')' || c == '*' || c == '+' || c == ',' || c == ';' || c == '=' ||
              c == '%')) {
            return false;
        }
    }
    return true;
}

bool DeviceFrameworkUI::setConfig(const DeviceFrameworkUIConfig& candidate) {
    const DeviceFrameworkBranding& branding = candidate.branding;
    if (locked || !isThemeValid(candidate.theme) ||
        !isTextValid(branding.brandName) || !isTextValid(branding.productName) ||
        !isTextValid(branding.webTitle) || !isTextValid(branding.provisioningTitle) ||
        !isTextValid(branding.provisioningTagline) || !isTextValid(branding.logoAltText) ||
        !isTextValid(candidate.about.summary) ||
        !isExternalLinkValid(candidate.about.primaryLink) ||
        !isExternalLinkValid(candidate.about.creditLink)) {
        return false;
    }

    config = candidate;
    configured = true;
    // Allocate the small presentation strings before WiFi and the web server
    // start. This avoids fragmenting the ESP8266 heap during a page response.
    rebuildWebThemeStyle();
    rebuildEscapedText();
    rebuildAboutContent();
    return true;
}

const DeviceFrameworkUIConfig& DeviceFrameworkUI::getConfig() {
    return config;
}

bool DeviceFrameworkUI::isConfigured() {
    return configured;
}

void DeviceFrameworkUI::applyPortalConfig(WiFiManager& wifiManager) {
    if (!configured) return;

    WiFiManagerPortalConfig portal;
    portal.title = mapPortalText(config.branding.provisioningTitle);
    portal.identityText = mapPortalText(config.branding.brandName);
    portal.tagline = mapPortalText(config.branding.provisioningTagline);
    portal.logo = {mapPortalText(config.branding.portalLogoSvg)};
    portal.logoAltText = mapPortalText(getLogoAltText());
    portal.theme.pageBackground = mapPortalText(config.theme.pageStart);
    portal.theme.surface = mapPortalText(config.theme.surface);
    portal.theme.text = mapPortalText(config.theme.text);
    portal.theme.mutedText = mapPortalText(config.theme.mutedText);
    portal.theme.border = mapPortalText(config.theme.border);
    portal.theme.accent = mapPortalText(config.theme.accent);
    portal.theme.accentHover = mapPortalText(config.theme.accentHover);
    portal.theme.accentText = mapPortalText(config.theme.accentText);
    portal.theme.danger = mapPortalText(config.theme.danger);
    portal.theme.success = mapPortalText(config.theme.success);
    portal.theme.cornerRadiusPx = config.theme.cornerRadiusPx;
    (void)wifiManager.setPortalConfig(portal);
}

void DeviceFrameworkUI::lock() {
    locked = true;
}

void DeviceFrameworkUI::rebuildWebThemeStyle() {
    webThemeStyle = "";
    const DeviceFrameworkTheme& theme = config.theme;
    const bool hasColours = !theme.pageStart.empty() || !theme.pageEnd.empty() ||
        !theme.surface.empty() || !theme.text.empty() || !theme.mutedText.empty() ||
        !theme.border.empty() || !theme.accent.empty() || !theme.accentHover.empty() ||
        !theme.accentText.empty() || !theme.success.empty() || !theme.danger.empty();
    if (!hasColours && theme.cornerRadiusPx == 0) return;

    size_t styleCapacity = strlen("<style id='df-web-theme'>:root{") + strlen("}</style>");
    const DeviceFrameworkText* values[] = {
        &theme.pageStart, &theme.pageEnd, &theme.surface, &theme.text,
        &theme.mutedText, &theme.border, &theme.accent, &theme.accentHover,
        &theme.accentText, &theme.success, &theme.danger,
    };
    const char* names[] = {
        "--df-page-start", "--df-page-end", "--df-surface", "--df-text",
        "--df-muted", "--df-border", "--df-accent", "--df-accent-hover",
        "--df-accent-text", "--df-success", "--df-danger",
    };
    for (size_t index = 0; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!values[index]->empty()) styleCapacity += strlen(names[index]) + values[index]->length() + 2;
    }
    if (theme.cornerRadiusPx > 0) styleCapacity += 48;

    webThemeStyle = "";
    webThemeStyle.reserve(styleCapacity);
    webThemeStyle = F("<style id='df-web-theme'>:root{");
    appendCssToken(webThemeStyle, "--df-page-start", theme.pageStart);
    appendCssToken(webThemeStyle, "--df-page-end", theme.pageEnd);
    appendCssToken(webThemeStyle, "--df-surface", theme.surface);
    appendCssToken(webThemeStyle, "--df-text", theme.text);
    appendCssToken(webThemeStyle, "--df-muted", theme.mutedText);
    appendCssToken(webThemeStyle, "--df-border", theme.border);
    appendCssToken(webThemeStyle, "--df-accent", theme.accent);
    appendCssToken(webThemeStyle, "--df-accent-hover", theme.accentHover);
    appendCssToken(webThemeStyle, "--df-accent-text", theme.accentText);
    appendCssToken(webThemeStyle, "--df-success", theme.success);
    appendCssToken(webThemeStyle, "--df-danger", theme.danger);
    if (theme.cornerRadiusPx > 0) {
        webThemeStyle += F("--df-radius:");
        webThemeStyle += String(theme.cornerRadiusPx);
        webThemeStyle += F("px;--df-radius-sm:");
        webThemeStyle += String(theme.cornerRadiusPx);
        webThemeStyle += F("px;--df-card-radius:");
        webThemeStyle += String(theme.cornerRadiusPx * 2);
        webThemeStyle += F("px;");
    }
    webThemeStyle += F("}</style>");
}

const char* DeviceFrameworkUI::getWebThemeStyle() {
    if (webThemeStyle.length() == 0) rebuildWebThemeStyle();
    return webThemeStyle.c_str();
}

void DeviceFrameworkUI::rebuildEscapedText() {
    escapedBrandName = "";
    escapedWebTitle = "";
    escapedLogoAltText = "";
    escapedBrandName.reserve(escapedTextLength(getBrandName()));
    escapedWebTitle.reserve(escapedTextLength(getWebTitle()));
    escapedLogoAltText.reserve(escapedTextLength(getLogoAltText()));
    appendHtmlEscaped(escapedBrandName, getBrandName());
    appendHtmlEscaped(escapedWebTitle, getWebTitle());
    appendHtmlEscaped(escapedLogoAltText, getLogoAltText());
}

void DeviceFrameworkUI::rebuildAboutContent() {
    aboutNavigation = "";
    aboutSection = "";
    const DeviceFrameworkAbout& about = config.about;
    const bool hasPrimary = !about.primaryLink.url.empty();
    const bool hasCredit = !about.creditLink.url.empty();
    if (about.summary.empty() && !hasPrimary && !hasCredit) return;

    // These small strings are built before WiFi and the web server start,
    // alongside the configured theme, so rendering does not fragment ESP8266
    // heap during a request.
    aboutNavigation = F("<a href=\"#about\" class=\"nav-link\">About</a>");
    aboutSection = F("<section class=\"card about-card\" id=\"about\"><h2>About</h2>");
    if (!about.summary.empty()) {
        aboutSection += F("<p class=\"about-summary\">");
        appendHtmlEscaped(aboutSection, about.summary);
        aboutSection += F("</p>");
    }
    if (hasPrimary || hasCredit) {
        aboutSection += F("<p class=\"about-links\">");
        if (hasPrimary) {
            aboutSection += F("<a href=\"");
            appendHtmlEscaped(aboutSection, about.primaryLink.url);
            aboutSection += F("\" target=\"_blank\" rel=\"noopener noreferrer\">");
            appendHtmlEscaped(aboutSection, about.primaryLink.label);
            aboutSection += F("</a>");
        }
        if (hasPrimary && hasCredit) aboutSection += F("<span aria-hidden=\"true\">&middot;</span>");
        if (hasCredit) {
            aboutSection += F("<a href=\"");
            appendHtmlEscaped(aboutSection, about.creditLink.url);
            aboutSection += F("\" target=\"_blank\" rel=\"noopener noreferrer\">");
            appendHtmlEscaped(aboutSection, about.creditLink.label);
            aboutSection += F("</a>");
        }
        aboutSection += F("</p>");
    }
    aboutSection += F("</section>");
}



const DeviceFrameworkText& DeviceFrameworkUI::getBrandName() {
    return config.branding.brandName;
}

const DeviceFrameworkText& DeviceFrameworkUI::getWebTitle() {
    return !config.branding.webTitle.empty()
        ? config.branding.webTitle
        : config.branding.productName;
}

const DeviceFrameworkText& DeviceFrameworkUI::getLogoAltText() {
    if (!config.branding.logoAltText.empty()) {
        return config.branding.logoAltText;
    }
    if (!config.branding.brandName.empty()) {
        return config.branding.brandName;
    }
    return defaultLogoAltText;
}

const char* DeviceFrameworkUI::getEscapedBrandName() {
    if (escapedBrandName.length() == 0 && !getBrandName().empty()) rebuildEscapedText();
    return escapedBrandName.c_str();
}

const char* DeviceFrameworkUI::getEscapedWebTitle() {
    if (escapedWebTitle.length() == 0 && !getWebTitle().empty()) rebuildEscapedText();
    return escapedWebTitle.c_str();
}

const char* DeviceFrameworkUI::getEscapedLogoAltText() {
    if (escapedLogoAltText.length() == 0 && !getLogoAltText().empty()) rebuildEscapedText();
    return escapedLogoAltText.c_str();
}

const DeviceFrameworkWebLogo& DeviceFrameworkUI::getWebLogo() {
    return config.branding.webLogo.base64Data ? config.branding.webLogo : defaultWebLogo;
}

const char* DeviceFrameworkUI::getAboutNavigation() {
    return aboutNavigation.c_str();
}

const char* DeviceFrameworkUI::getAboutSection() {
    return aboutSection.c_str();
}
