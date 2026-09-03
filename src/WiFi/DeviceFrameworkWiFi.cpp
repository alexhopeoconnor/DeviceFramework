#include "DeviceFrameworkWiFi.h"
#include <WiFiManager.h>
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../DeviceFramework.h"
#include "../Provisioning/DeviceFrameworkProvisioning.h"
#include "../UI/DeviceFrameworkUI.h"
#include "../WebInterface/WebInterfaceTemplateEngineLogger.h"
#ifdef ENABLE_WEB_INTERFACE
#include "../WebInterface/DeviceFrameworkWeb.h"
#endif
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
DeviceFrameworkWiFiProfileStore DeviceFrameworkWiFi::profileStore;
bool DeviceFrameworkWiFi::provisioningCandidatePending = false;
WiFiManagerStationProfiles DeviceFrameworkWiFi::provisioningCandidate;

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


    // The framework owns product-level UI settings and maps them to
    // WiFiManager's independent portal API before the portal can start.
    DeviceFrameworkUI::applyPortalConfig(wm);
    DeviceFrameworkUI::lock();
    // Create WiFiManagerParameters from registry
    auto wifiParams = registry->createWiFiManagerParameters();

    // Add all parameters to WiFiManager
    for (size_t i = 0; i < wifiParams.count; i++) {
        wm.portalAddParameter(wifiParams.parameters[i]);
    }

    // Profile mode owns both profile selection and reconnect attempts. ESP
    // Wi-Fi has one active station configuration, so it must not persist or
    // auto-reconnect a competing SDK-owned credential.
    wm.setStationProfileStore(&profileStore);
    wm.setStationRecoveryInterval(getConfigWiFiReconnectInterval());
    wm.setEventCallback(stationEventCallbackInternal);
    wm.setEnableConfigPortal(true);

    // Configure WiFiManager (non-blocking mode is default with ESPAsyncWebServer)
    wm.setConfigPortalTimeout(CONFIG_configModeTimeout / 1000);

    // Set parameter save callback - saves params immediately (even if WiFi fails)
    wm.setSaveParamsCallback(saveParamsCallbackInternal);

    // Set WiFi save callback - called after WiFi connects successfully
    wm.setSaveConfigCallback(saveConfigCallbackInternal);

    // Use a lambda wrapper to adapt WiFiManager's callback signature.
    wm.setAPCallback([](WiFiManager* wifiManager) {
        (void)wifiManager;
        DeviceFrameworkWiFi::configModeCallbackInternal();
    });


    // Start the shared non-blocking profile controller. A provisioned profile
    // remains an in-memory candidate until it has produced a usable IP.
    LOG_INFOLN(F("Starting WiFi profile controller..."));
    String apName = registry->getValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME);
    const bool started = provisioningCandidatePending
        ? wm.startStationCandidate(provisioningCandidate, apName.c_str(), DeviceFramework::getDevicePassword())
        : wm.startStationConnection(apName.c_str(), DeviceFramework::getDevicePassword());
    if (!started && wm.getConfigPortalActive()) {
        LOG_WARNLN(F("No usable WiFi profile - entering config mode"));
        isConfigMode = true;
        setupWebInterfaceTemplateEngineLogging();
    }
    return started;
}

void DeviceFrameworkWiFi::preloadWiFi(const char* ssid, const char* password) {
    if (!ssid || !ssid[0]) return;
    WiFiManagerStationProfiles profiles;
    WiFiManagerStationProfile& profile = profiles.slots[0];
    profile.enabled = true;
    profile.hasPassword = password && password[0];
    strncpy(profile.ssid, ssid, sizeof(profile.ssid) - 1);
    if (profile.hasPassword) strncpy(profile.password, password, sizeof(profile.password) - 1);
    setProvisioningCandidate(profiles);
}

void DeviceFrameworkWiFi::setProvisioningCandidate(const WiFiManagerStationProfiles& profiles) {
    provisioningCandidate = profiles;
    provisioningCandidatePending = true;
}

void DeviceFrameworkWiFi::clearProfiles() {
    if (wm.isStationProfileMode()) {
        wm.clearStationProfiles();
        if (wm.getStationStatus().storageSaveFailed) {
            LOG_ERRORLN(F("Unable to clear persisted WiFi profiles"));
            return;
        }
    } else {
        if (!DeviceFrameworkStorage::saveWithStationProfiles(WiFiManagerStationProfiles())) {
            LOG_ERRORLN(F("Unable to clear persisted WiFi profiles"));
        }
        wm.resetSettings();
    }
    provisioningCandidate = WiFiManagerStationProfiles();
    provisioningCandidatePending = false;
}

void DeviceFrameworkWiFi::loop() {
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

    if (!hasUsableConnection()) {
        // Blink LED rapidly to indicate offline state (if available)
        if ((lastConfigLEDToggle + (CONFIG_configLEDToggleRate / 4)) < currentMillis) {
            configLEDState = !configLEDState;
            LED_SET(configLEDState);
            lastConfigLEDToggle = currentMillis;
        }
        return;
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

bool DeviceFrameworkWiFi::hasUsableConnection() {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
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
#ifdef ENABLE_WEB_INTERFACE
    // A failed asynchronous profile candidate can reach the portal after the
    // normal web server has started. Release port 80 before WiFiManager
    // registers its own portable provisioning routes.
    if (DeviceFrameworkWeb::isEnabled()) {
        DeviceFrameworkWeb::shutdown();
    }
#endif
    isConfigMode = true;          // Enter config mode
    isConfigAttempted = true;     // Track that config was attempted
    DeviceFrameworkWiFi::wm.setEnableConfigPortal(false);  // Prevent future portal attempts in this cycle

    // Call user-defined config mode callback, if registered
    if (userConfigModeCallback) {
        userConfigModeCallback();
    }
}

void DeviceFrameworkWiFi::stationEventCallbackInternal(WiFiManager::wm_event_t event) {
    if (event != WiFiManager::WM_EVENT_STATION_PROFILE_CONNECTED) return;

    const WiFiManager::wm_station_status_t& status = wm.getStationStatus();
    if (status.lastConnectionWasCandidate && !status.storageSaveFailed) {
        DeviceFrameworkProvisioning::markConnectionSucceeded();
    }
    provisioningCandidatePending = false;
}
