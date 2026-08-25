#include "DeviceFrameworkProvisioning.h"

#include "../Configuration/DeviceFrameworkIdentity.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../Configuration/DeviceFrameworkParameterRegistry.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../DeviceFrameworkDebug.h"

#ifndef DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
#define DEVICEFRAMEWORK_HAS_LOCAL_PROFILE 0
#endif

#if DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
#include "DeviceFrameworkLocalProfile.h"
#endif

namespace {
#if DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
uint32_t profileHash(const char* value) {
    uint32_t hash = 2166136261UL;
    for (const char* cursor = value; cursor && *cursor; ++cursor) {
        hash ^= static_cast<uint8_t>(*cursor);
        hash *= 16777619UL;
    }
    return hash;
}
#endif

bool isProfileEnabled() {
#if DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
    return DEVICEFRAMEWORK_PROFILE_ID[0] != 0;
#else
    return false;
#endif
}
}

uint32_t DeviceFrameworkProvisioning::activeProfileHash = 0;
uint32_t DeviceFrameworkProvisioning::activeProfileRevision = 0;
bool DeviceFrameworkProvisioning::pendingWiFi = false;
const char* DeviceFrameworkProvisioning::pendingWiFiSSID = "";
const char* DeviceFrameworkProvisioning::pendingWiFiPassword = "";

bool DeviceFrameworkProvisioning::apply(const DeviceFrameworkStorageLoadResult& storageResult) {
    if (!isProfileEnabled()) return false;

#if DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
    activeProfileHash = profileHash(DEVICEFRAMEWORK_PROFILE_ID);
    if (DeviceFrameworkIdentity::getApplication().applicationId != DEVICEFRAMEWORK_PROFILE_APPLICATION) {
        LOG_WARNLN(F("DeviceFramework profile ignored: application ID does not match firmware"));
        return false;
    }

    activeProfileRevision = DEVICEFRAMEWORK_PROFILE_REVISION;
    // The shared device password is runtime configuration for this firmware,
    // not a provisioned parameter. Apply it every boot, independently of the
    // profile policy that decides whether stored WiFi/parameter values change.
    if (!setConfigDevicePassword(DEVICEFRAMEWORK_PROFILE_DEVICE_PASSWORD)) {
        LOG_WARNLN(F("DeviceFramework profile: rejected device password (must be 8-31 characters)"));
    }
    const DeviceFrameworkProvisioningState previous = DeviceFrameworkStorage::getProvisioningState();
    const bool bootstrap = DEVICEFRAMEWORK_PROFILE_POLICY == DEVICEFRAMEWORK_PROFILE_BOOTSTRAP;
    const bool applyNow = bootstrap
        ? storageResult.status == DeviceFrameworkStorageLoadStatus::Empty ||
              storageResult.status == DeviceFrameworkStorageLoadStatus::ForeignApplication
        : previous.profileHash != activeProfileHash || previous.attemptedRevision != activeProfileRevision;
    if (!applyNow) return false;

    DeviceFrameworkProvisioningState next = previous;
    next.profileHash = activeProfileHash;
    next.attemptedRevision = activeProfileRevision;
    DeviceFrameworkStorage::setProvisioningState(next);

    DeviceFrameworkParameterRegistry& registry = DeviceFrameworkParameters::getRegistry();
    for (size_t index = 0; index < DEVICEFRAMEWORK_PROFILE_PARAMETER_COUNT; ++index) {
        const DeviceFrameworkProvisionedParameter& parameter = DEVICEFRAMEWORK_PROFILE_PARAMETERS[index];
        if (!registry.setValue(parameter.id, parameter.value, DeviceFrameworkParameterUpdateOrigin::DEVICE)) {
            LOG_WARN_SP(F("DeviceFramework profile: unknown or invalid parameter "), true);
            LOG_WARNLN_SP(parameter.id, false);
        }
    }

    if (DEVICEFRAMEWORK_PROFILE_WIFI_SSID[0]) {
        pendingWiFi = true;
        pendingWiFiSSID = DEVICEFRAMEWORK_PROFILE_WIFI_SSID;
        pendingWiFiPassword = DEVICEFRAMEWORK_PROFILE_WIFI_PASSWORD;
    }
    LOG_INFOLN(F("DeviceFramework profile applied"));
    return true;
#else
    (void)storageResult;
    return false;
#endif
}

void DeviceFrameworkProvisioning::markConnectionSucceeded() {
    if (!pendingWiFi) return;
    DeviceFrameworkProvisioningState state = DeviceFrameworkStorage::getProvisioningState();
    state.profileHash = activeProfileHash;
    state.appliedRevision = activeProfileRevision;
    DeviceFrameworkStorage::setProvisioningState(state);
    DeviceFrameworkStorage::save();
    pendingWiFi = false;
}

bool DeviceFrameworkProvisioning::hasPendingWiFi() {
    return pendingWiFi;
}

const char* DeviceFrameworkProvisioning::getPendingWiFiSSID() {
    return pendingWiFiSSID;
}

const char* DeviceFrameworkProvisioning::getPendingWiFiPassword() {
    return pendingWiFiPassword;
}
