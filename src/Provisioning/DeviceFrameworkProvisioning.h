#ifndef DEVICEFRAMEWORK_PROVISIONING_H
#define DEVICEFRAMEWORK_PROVISIONING_H

#include <Arduino.h>

#include "../Storage/DeviceFrameworkStorage.h"

enum DeviceFrameworkProfilePolicy {
    DEVICEFRAMEWORK_PROFILE_BOOTSTRAP,
    DEVICEFRAMEWORK_PROFILE_RECONCILE
};

struct DeviceFrameworkProvisionedParameter {
    const char* id;
    const char* value;
};

class DeviceFrameworkProvisioning {
public:
    // Loads the optional build-local shared device password on every boot.
    // Bootstrap seeds empty storage or a valid record for another application;
    // reconcile applies WiFi and parameter values once for each new profile
    // revision.
    static bool apply(const DeviceFrameworkStorageLoadResult& storageResult);
    static void markConnectionSucceeded();
    static bool hasPendingWiFi();
    static const char* getPendingWiFiSSID();
    static const char* getPendingWiFiPassword();

private:
    static uint32_t activeProfileHash;
    static uint32_t activeProfileRevision;
    static bool pendingWiFi;
    static const char* pendingWiFiSSID;
    static const char* pendingWiFiPassword;
};

#endif // DEVICEFRAMEWORK_PROVISIONING_H
