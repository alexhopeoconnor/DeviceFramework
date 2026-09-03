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
WiFiManagerStationProfiles DeviceFrameworkProvisioning::pendingWiFiProfiles;

bool DeviceFrameworkProvisioning::apply(const DeviceFrameworkStorageLoadResult& storageResult) {
    if (!isProfileEnabled()) return false;

#if DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
    activeProfileHash = profileHash(DEVICEFRAMEWORK_PROFILE_ID);
    if (DeviceFrameworkIdentity::getApplication().applicationId != DEVICEFRAMEWORK_PROFILE_APPLICATION) {
        LOG_WARNLN(F("DeviceFramework profile ignored: application ID does not match firmware"));
        return false;
    }

    activeProfileRevision = DEVICEFRAMEWORK_PROFILE_REVISION;
    const DeviceFrameworkProvisioningState previous = DeviceFrameworkStorage::getProvisioningState();
    const bool bootstrap = DEVICEFRAMEWORK_PROFILE_POLICY == DEVICEFRAMEWORK_PROFILE_BOOTSTRAP;
    const bool initialConfiguration =
        storageResult.status == DeviceFrameworkStorageLoadStatus::Empty ||
        storageResult.status == DeviceFrameworkStorageLoadStatus::ForeignApplication ||
        storageResult.status == DeviceFrameworkStorageLoadStatus::UnsupportedLegacyFormat;

    const bool applyNow = bootstrap
        ? initialConfiguration
        : initialConfiguration || previous.profileHash != activeProfileHash ||
              previous.attemptedRevision != activeProfileRevision;

    // bootstrap profiles seed only a new/recovery configuration. An explicit
    // reconcile profile owns every supplied managed value, including the one
    // shared password, exactly once per profile ID/revision. Omitted passwords
    // never clear an existing runtime-managed value.
    const bool applyProfilePassword = DEVICEFRAMEWORK_PROFILE_HAS_DEVICE_PASSWORD &&
        (initialConfiguration || storageResult.status == DeviceFrameworkStorageLoadStatus::Corrupt ||
         (!bootstrap && applyNow));
    if (applyProfilePassword && !setConfigDevicePassword(DEVICEFRAMEWORK_PROFILE_DEVICE_PASSWORD)) {
        LOG_WARNLN(F("DeviceFramework profile: rejected device password (must be empty or 8-31 characters)"));
        return false;
    }

    // Corrupt or schema-incompatible V4 data must not be overwritten by an
    // automatic profile reconcile. Corrupt data still receives the RAM-only
    // profile fallback so the local recovery surfaces remain usable.
    if (!initialConfiguration && !storageResult.hasUsableConfiguration()) return false;

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

    pendingWiFiProfiles = WiFiManagerStationProfiles();
    for (size_t index = 0; index < DEVICEFRAMEWORK_PROFILE_WIFI_PROFILE_COUNT; ++index) {
        const DeviceFrameworkProvisionedWiFiProfile& source = DEVICEFRAMEWORK_PROFILE_WIFI_PROFILES[index];
        WiFiManagerStationProfile& target = pendingWiFiProfiles.slots[index];
        target.enabled = true;
        target.hasPassword = source.password[0] != 0;
        strncpy(target.ssid, source.ssid, sizeof(target.ssid) - 1);
        if (target.hasPassword) {
            strncpy(target.password, source.password, sizeof(target.password) - 1);
        }
    }
    if (DEVICEFRAMEWORK_PROFILE_WIFI_PROFILE_COUNT > 0) {
        pendingWiFi = true;
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
    if (!DeviceFrameworkStorage::save()) {
        LOG_ERRORLN(F("DeviceFramework profile: unable to mark WiFi candidate applied"));
        return;
    }
    pendingWiFi = false;
}

bool DeviceFrameworkProvisioning::hasPendingWiFi() {
    return pendingWiFi;
}

const WiFiManagerStationProfiles& DeviceFrameworkProvisioning::getPendingWiFiProfiles() {
    return pendingWiFiProfiles;
}
