#ifndef DEVICE_FRAMEWORK_WIFI_H
#define DEVICE_FRAMEWORK_WIFI_H

#include <WiFiManager.h>
#include <vector>
#include <map>
#include <Arduino.h>
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../Configuration/DeviceFrameworkParameterRegistry.h"
#include "DeviceFrameworkWiFiProfileStore.h"
#include "../DeviceFrameworkDebug.h"

// Forward declarations
class DeviceFrameworkParameterRegistry;
class WiFiManager;

class DeviceFrameworkWiFi {
private:
    static DeviceFrameworkParameterRegistry* registry;
    static WiFiManager wm;
    static DeviceFrameworkWiFiProfileStore profileStore;
    static bool provisioningCandidatePending;
    static WiFiManagerStationProfiles provisioningCandidate;

    // State for config mode
    static bool isConfigMode;
    static bool configLEDState;
    static unsigned long lastConfigLEDToggle;
    static bool isConfigAttempted;

    // User-defined callbacks
    static void (*userSaveParamsCallback)(WiFiManager::WiFiManagerRequestArgs requestArgs);
    static void (*userSaveConfigCallback)();
    static void (*userConfigModeCallback)();

    // Internal callbacks
    static void saveParamsCallbackInternal(WiFiManager::WiFiManagerRequestArgs requestArgs);
    static void saveConfigCallbackInternal();
    static void configModeCallbackInternal();
    static void stationEventCallbackInternal(WiFiManager::wm_event_t event);

public:
    static bool setup();
    static void preloadWiFi(const char* ssid, const char* password);
    static void setProvisioningCandidate(const WiFiManagerStationProfiles& profiles);
    static void clearProfiles();
    static void loop();

    // State management
    static bool isInConfigMode();
    static bool getConfigAttempted();
    static bool hasUsableConnection();

    // Callback management
    static void setSaveParamsCallback(void (*callback)(WiFiManager::WiFiManagerRequestArgs requestArgs));
    static void setSaveConfigCallback(void (*callback)());
    static void setConfigModeCallback(void (*callback)());

    // WiFiManager access
    static WiFiManager& getWiFiManager();
};

#endif // DEVICE_FRAMEWORK_WIFI_H
