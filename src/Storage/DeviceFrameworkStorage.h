#ifndef DEVICEFRAMEWORK_STORAGE_H
#define DEVICEFRAMEWORK_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>

#include <map>

#include "../Configuration/DeviceFrameworkConfig.h"

enum class DeviceFrameworkStorageLoadStatus {
    Loaded,
    Migrated,
    LegacyImported,
    Empty,
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
               status == DeviceFrameworkStorageLoadStatus::Migrated ||
               status == DeviceFrameworkStorageLoadStatus::LegacyImported;
    }
};

class DeviceFrameworkStorage {
public:
    static constexpr uint16_t STORAGE_FORMAT_VERSION = 2;

    static void setup();
    static bool save();
    static DeviceFrameworkStorageLoadResult load();
    static bool reset();
    static const DeviceFrameworkProvisioningState& getProvisioningState();
    static void setProvisioningState(const DeviceFrameworkProvisioningState& state);
    static const DeviceFrameworkStorageLoadResult& getLastLoadResult();

private:
    static DeviceFrameworkStorageLoadResult lastLoadResult;
    static DeviceFrameworkProvisioningState provisioningState;
    static bool readV2(std::map<String, String>& values, uint16_t& schema, uint32_t& generation);
    static bool readLegacyV1(std::map<String, String>& values);
    static bool applyValues(const std::map<String, String>& values);
};

#endif // DEVICEFRAMEWORK_STORAGE_H
