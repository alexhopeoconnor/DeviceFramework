#ifndef DEVICEFRAMEWORK_IDENTITY_H
#define DEVICEFRAMEWORK_IDENTITY_H

#include <Arduino.h>
#include <map>

// This value is verified against library.json by scripts/prepare-release.sh.
#ifndef DEVICEFRAMEWORK_LIBRARY_VERSION
#define DEVICEFRAMEWORK_LIBRARY_VERSION "2.7.0"
#endif

class DeviceFrameworkConfigMigration {
public:
    explicit DeviceFrameworkConfigMigration(std::map<String, String>& values)
        : values(values) {}

    bool has(const char* id) const;
    bool rename(const char* from, const char* to);
    bool remove(const char* id);
    bool setIfMissing(const char* id, const char* value);
    bool multiplyUInt(const char* id, uint32_t multiplier);
    bool clampUInt(const char* id, uint32_t minimum, uint32_t maximum, const char* fallback);

private:
    std::map<String, String>& values;
};

typedef bool (*DeviceFrameworkConfigMigrationCallback)(
    uint16_t fromSchema,
    DeviceFrameworkConfigMigration& migration
);

struct DeviceFrameworkApplicationIdentity {
    String applicationId;
    String firmwareVersion;
    uint16_t configurationSchema;
    DeviceFrameworkConfigMigrationCallback migration;

    DeviceFrameworkApplicationIdentity()
        : applicationId("deviceframework"), firmwareVersion("0.0.0-dev"),
          configurationSchema(1), migration(nullptr) {}
};

class DeviceFrameworkIdentity {
public:
    static const char* getLibraryVersion();
    static const DeviceFrameworkApplicationIdentity& getApplication();
    static bool configureApplication(const char* applicationId, const char* firmwareVersion,
                                     uint16_t configurationSchema,
                                     DeviceFrameworkConfigMigrationCallback migration = nullptr);

private:
    static DeviceFrameworkApplicationIdentity application;
};

#endif // DEVICEFRAMEWORK_IDENTITY_H
