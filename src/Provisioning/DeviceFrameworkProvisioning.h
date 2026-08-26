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
    // A selected profile seeds an empty/foreign/DFC2 record and provides a
    // RAM-only password fallback for corrupt storage. Valid V3 records retain
    // their password; reconcile applies WiFi and parameter values once per
    // profile revision.
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
