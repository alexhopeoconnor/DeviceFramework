#ifndef DEVICEFRAMEWORK_STORAGE_H
#define DEVICEFRAMEWORK_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <WiFiManager.h>

#include <map>

#include "../Configuration/DeviceFrameworkConfig.h"

enum class DeviceFrameworkStorageLoadStatus {
    Loaded,
    Migrated,
    Empty,
    // A valid V4 record belongs to a different APPLICATION_ID. Its values are
    // deliberately not loaded into this firmware, but an explicit local
    // bootstrap profile may safely seed this application without an erase.
    ForeignApplication,
    // DFC2/DFC3 are recognised only so a selected bootstrap profile can write fresh
    // DFC4 data. DeviceFramework does not decode or migrate them.
    UnsupportedLegacyFormat,
    Corrupt,
    Incompatible
};

struct DeviceFrameworkProvisioningState {
    uint32_t profileHash;
    uint32_t attemptedRevision;
    uint32_t appliedRevision;
    DeviceFrameworkProvisioningState() : profileHash(0), attemptedRevision(0), appliedRevision(0) {}
};

struct DeviceFrameworkStorageLoadResult {
    DeviceFrameworkStorageLoadStatus status;
    bool requiresSave;

    DeviceFrameworkStorageLoadResult(
        DeviceFrameworkStorageLoadStatus status = DeviceFrameworkStorageLoadStatus::Empty,
        bool requiresSave = false
    ) : status(status), requiresSave(requiresSave) {}

    bool hasUsableConfiguration() const {
        return status == DeviceFrameworkStorageLoadStatus::Loaded ||
               status == DeviceFrameworkStorageLoadStatus::Migrated;
    }
};

class DeviceFrameworkStorage {
public:
    static constexpr uint16_t STORAGE_FORMAT_VERSION = 4;

    static void setup();
    static bool save();
    // Writes a complete current record using a candidate password, but does not
    // mutate runtime state. Callers activate it only after verification.
    static bool saveWithDevicePassword(const char* password);
    static bool saveWithStationProfiles(const WiFiManagerStationProfiles& profiles);
    static DeviceFrameworkStorageLoadResult load();
    static bool reset();
    static const DeviceFrameworkProvisioningState& getProvisioningState();
    static void setProvisioningState(const DeviceFrameworkProvisioningState& state);
    static const WiFiManagerStationProfiles& getStationProfiles();
    static void setStationProfiles(const WiFiManagerStationProfiles& profiles);
    static const DeviceFrameworkStorageLoadResult& getLastLoadResult();

private:
    static DeviceFrameworkStorageLoadResult lastLoadResult;
    static DeviceFrameworkProvisioningState provisioningState;
    static WiFiManagerStationProfiles stationProfiles;
    static bool readCurrent(std::map<String, String>& values, String& password,
                       uint16_t& schema, uint32_t& generation);
    static bool applyValues(const std::map<String, String>& values);
};

#endif // DEVICEFRAMEWORK_STORAGE_H
