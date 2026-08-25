#include "DeviceFrameworkWiFi.h"
#include <WiFiManager.h>
#include "../Utils/TimeUtils.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../DeviceFramework.h"
#include "../WebInterface/templates/WiFiManagerStyles.h"
#include "../WebInterface/WebInterfaceTemplateEngineLogger.h"
#include <DeviceFrameworkPlatform.h>  // Platform abstraction

// LED helper macros - simplify LED control code
#if DF_LED_AVAILABLE
    #if DF_LED_ACTIVE_LOW
        #define LED_WRITE_ON()  digitalWrite(DF_LED_PIN, LOW)
        #define LED_WRITE_OFF() digitalWrite(DF_LED_PIN, HIGH)
    #else
        #define LED_WRITE_ON()  digitalWrite(DF_LED_PIN, HIGH)
        #define LED_WRITE_OFF() digitalWrite(DF_LED_PIN, LOW)
    #endif
    #define LED_INIT()        pinMode(DF_LED_PIN, OUTPUT); LED_WRITE_OFF()
    #define LED_ON()          LED_WRITE_ON()
    #define LED_OFF()         LED_WRITE_OFF()
    #define LED_SET(state)    digitalWrite(DF_LED_PIN, (state) ? (DF_LED_ACTIVE_LOW ? LOW : HIGH) : (DF_LED_ACTIVE_LOW ? HIGH : LOW))
#else
    #define LED_INIT()        do {} while(0)
    #define LED_ON()          do {} while(0)
    #define LED_OFF()         do {} while(0)
    #define LED_SET(state)    do {} while(0)
#endif

// Static member initialization
DeviceFrameworkParameterRegistry* DeviceFrameworkWiFi::registry = nullptr;
WiFiManager DeviceFrameworkWiFi::wm;

// Config mode state
bool DeviceFrameworkWiFi::isConfigMode = false;
bool DeviceFrameworkWiFi::configLEDState = false;
unsigned long DeviceFrameworkWiFi::lastConfigLEDToggle = 0;
bool DeviceFrameworkWiFi::isConfigAttempted = false;

// User-defined callbacks
void (*DeviceFrameworkWiFi::userSaveParamsCallback)(WiFiManager::WiFiManagerRequestArgs requestArgs) = nullptr;
void (*DeviceFrameworkWiFi::userSaveConfigCallback)() = nullptr;
void (*DeviceFrameworkWiFi::userConfigModeCallback)() = nullptr;

bool DeviceFrameworkWiFi::setup() {
    // Get registry reference from DeviceFrameworkParameters
    registry = &DeviceFrameworkParameters::getRegistry();

    // Initialize onboard LED for config mode indication (if available)
    LED_INIT();

    // Create WiFiManagerParameters from registry
    auto wifiParams = registry->createWiFiManagerParameters();

    // Add all parameters to WiFiManager
    for (size_t i = 0; i < wifiParams.count; i++) {
        wm.portalAddParameter(wifiParams.parameters[i]);
    }

    // Enable auto-reconnect for WiFi
    #ifdef DF_PLATFORM_ESP8266
        // ESP8266 supports WiFi.persistent()
        WiFi.persistent(true);
    #endif
    WiFi.setAutoReconnect(true);

    // Configure WiFiManager (non-blocking mode is default with ESPAsyncWebServer)
    wm.setConfigPortalTimeout(CONFIG_configModeTimeout / 1000);

    // Set parameter save callback - saves params immediately (even if WiFi fails)
    wm.setSaveParamsCallback(saveParamsCallbackInternal);

    // Set WiFi save callback - called after WiFi connects successfully
    wm.setSaveConfigCallback(saveConfigCallbackInternal);

    // Use lambda wrapper to adapt WiFiManager's callback signature (requires WiFiManager*)
    // to our internal callback that doesn't need it (we use our static wm instance)
    wm.setAPCallback([](WiFiManager* wm) {
        (void)wm; // Unused - we use our static instance instead
        DeviceFrameworkWiFi::configModeCallbackInternal();
    });

    // Apply custom styling to match web interface
    //wm.setCustomHeadElement(wifimanager_custom_css);

    // Attempt to auto-connect to previously saved network
    LOG_INFOLN(F("Starting WiFiManager..."));
    String apName = registry->getValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME);
    if (!wm.autoConnect(apName.c_str(), getConfigDevicePassword())) {
        LOG_WARNLN(F("Initial WiFi connection failed - entering config mode"));
        isConfigMode = true;
        setupWebInterfaceTemplateEngineLogging();
        return false;
    } else {
        LOG_INFOLN(F("Connected to WiFi on startup"));
        isConfigMode = false;
    }
    return true;
}

void DeviceFrameworkWiFi::preloadWiFi(const char* ssid, const char* password) {
    if (!ssid || !ssid[0]) return;
    wm.preloadWiFi(ssid, password ? password : "");
}

void DeviceFrameworkWiFi::loop() {
    // Process WiFiManager tasks (may block briefly; shutdownConfigPortal uses delay(1000))
    // Process WiFiManager tasks (may block briefly; shutdownConfigPortal uses delay(1000))
    wm.process();

    // Timing must use millis() after process(): stale "currentMillis" caused offline LED blink
    // to stall after portal timeout because interval compares used pre-process time.
    unsigned long currentMillis = millis();

    // Handle WiFiManager portal activity
    if (wm.getConfigPortalActive()) {
        // Blink the onboard LED in config mode (if available)
        if ((lastConfigLEDToggle + CONFIG_configLEDToggleRate) < currentMillis) {
            configLEDState = !configLEDState;
            LED_SET(configLEDState);
            lastConfigLEDToggle = currentMillis;
        }

        return; // Exit early if still in config mode
    }

    if (wm.hasEnteredConfigPortal() && wm.didConfigPortalConnectSucceed()
            && wm.getConfigPortalConnectStatus() == WL_CONNECTED) {
        LOG_INFOLN(F("Config portal connect succeeded - restarting device..."));
        ESP.restart();
        return;
    }

    // Ensure config mode state is reset when portal is closed
    if (isConfigMode) {
        isConfigMode = false;
        configLEDState = false;
        LED_OFF();
        // Allow rapid offline blink to start on the next line (avoids one dead interval if
        // lastConfigLEDToggle equals currentMillis or is only slightly earlier).
        if (CONFIG_configLEDToggleRate > 4) {
            lastConfigLEDToggle = currentMillis - (CONFIG_configLEDToggleRate / 4) - 1;
        } else {
            lastConfigLEDToggle = 0;
        }
        #if DF_LED_AVAILABLE
            LOG_DEBUGLN(F("Exiting config mode. Turning off onboard LED."));
        #endif
    }

    // Handle WiFi reconnection
    if (WiFi.status() != WL_CONNECTED) {
        // Blink LED rapidly to indicate offline state (if available)
        if ((lastConfigLEDToggle + (CONFIG_configLEDToggleRate / 4)) < currentMillis) {
            configLEDState = !configLEDState;
            LED_SET(configLEDState);
            lastConfigLEDToggle = currentMillis;
        }

        // Periodically attempt reconnection
        static unsigned long lastReconnectAttempt = 0;
        if (TimeUtils::hasTimeElapsed(currentMillis, lastReconnectAttempt, CONFIG_wifiReconnectInterval)) {
            LOG_WARNLN(F("WiFi disconnected - attempting reconnection..."));
            WiFi.reconnect();
            lastReconnectAttempt = currentMillis;
        }

        return; // Skip further processing until WiFi is connected
    }

    // Ensure the LED is off when WiFi is connected
    if (configLEDState) {
        configLEDState = false;
        LED_OFF();
        #if DF_LED_AVAILABLE
            LOG_INFOLN(F("WiFi connected. Turning off onboard LED..."));
        #endif
    }
}

bool DeviceFrameworkWiFi::isInConfigMode() {
    return isConfigMode;
}

bool DeviceFrameworkWiFi::getConfigAttempted() {
    return isConfigAttempted;
}

void DeviceFrameworkWiFi::setSaveParamsCallback(void (*callback)(WiFiManager::WiFiManagerRequestArgs requestArgs)) {
    userSaveParamsCallback = callback;
}

void DeviceFrameworkWiFi::setSaveConfigCallback(void (*callback)()) {
    userSaveConfigCallback = callback;
}

void DeviceFrameworkWiFi::setConfigModeCallback(void (*callback)()) {
    userConfigModeCallback = callback;
}

WiFiManager& DeviceFrameworkWiFi::getWiFiManager() {
    return wm;
}

// Parameter save callback - called immediately when params are saved
void DeviceFrameworkWiFi::saveParamsCallbackInternal(WiFiManager::WiFiManagerRequestArgs requestArgs) {
    // Sync values from RequestArgs to registry
    if (registry) {
        registry->syncFromWiFiManager(requestArgs);

        // Save parameters to EEPROM immediately
        registry->saveToStorage();

        LOG_INFOLN(F("Parameters saved to EEPROM"));
    }

    // Call user-defined save params callback if registered
    if (userSaveParamsCallback) {
        userSaveParamsCallback(requestArgs);
    }
}

// WiFi save callback - called after portal WiFi save flow reports success
void DeviceFrameworkWiFi::saveConfigCallbackInternal() {
    if (userSaveConfigCallback) {
        userSaveConfigCallback();
    }
}

void DeviceFrameworkWiFi::configModeCallbackInternal() {
    LOG_INFOLN(F("[CALLBACK] Configuration portal started."));
    isConfigMode = true;          // Enter config mode
    isConfigAttempted = true;     // Track that config was attempted
    DeviceFrameworkWiFi::wm.setEnableConfigPortal(false);  // Prevent future portal attempts in this cycle

    // Call user-defined config mode callback, if registered
    if (userConfigModeCallback) {
        userConfigModeCallback();
    }
}