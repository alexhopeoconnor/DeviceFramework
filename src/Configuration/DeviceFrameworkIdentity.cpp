#include "DeviceFrameworkIdentity.h"

#include <stdlib.h>

DeviceFrameworkApplicationIdentity DeviceFrameworkIdentity::application;

bool DeviceFrameworkConfigMigration::has(const char* id) const {
    return id && values.find(String(id)) != values.end();
}

bool DeviceFrameworkConfigMigration::rename(const char* from, const char* to) {
    if (!from || !to || !from[0] || !to[0]) return false;
    const String source(from);
    const String destination(to);
    const auto sourceIt = values.find(source);
    if (sourceIt == values.end()) return true;
    if (values.find(destination) == values.end()) values[destination] = sourceIt->second;
    values.erase(sourceIt);
    return true;
}

bool DeviceFrameworkConfigMigration::remove(const char* id) {
    if (!id || !id[0]) return false;
    values.erase(String(id));
    return true;
}

bool DeviceFrameworkConfigMigration::setIfMissing(const char* id, const char* value) {
    if (!id || !id[0] || !value) return false;
    if (!has(id)) values[String(id)] = String(value);
    return true;
}

bool DeviceFrameworkConfigMigration::multiplyUInt(const char* id, uint32_t multiplier) {
    if (!id || !id[0]) return false;
    const auto it = values.find(String(id));
    if (it == values.end()) return true;
    char* end = nullptr;
    const unsigned long current = strtoul(it->second.c_str(), &end, 10);
    if (end == it->second.c_str() || *end != '\0' || (multiplier && current > UINT32_MAX / multiplier)) return false;
    it->second = String(static_cast<uint32_t>(current) * multiplier);
    return true;
}

bool DeviceFrameworkConfigMigration::clampUInt(const char* id, uint32_t minimum, uint32_t maximum, const char* fallback) {
    if (!id || !id[0] || !fallback || minimum > maximum) return false;
    const auto it = values.find(String(id));
    if (it == values.end()) return true;
    char* end = nullptr;
    const unsigned long current = strtoul(it->second.c_str(), &end, 10);
    if (end == it->second.c_str() || *end != '\0' || current < minimum || current > maximum) it->second = String(fallback);
    return true;
}

const char* DeviceFrameworkIdentity::getLibraryVersion() { return DEVICEFRAMEWORK_LIBRARY_VERSION; }
const DeviceFrameworkApplicationIdentity& DeviceFrameworkIdentity::getApplication() { return application; }

bool DeviceFrameworkIdentity::configureApplication(const char* applicationId, const char* firmwareVersion,
                                                    uint16_t configurationSchema,
                                                    DeviceFrameworkConfigMigrationCallback migration) {
    if (!applicationId || !applicationId[0] || !firmwareVersion || !firmwareVersion[0] || configurationSchema == 0) return false;
    application.applicationId = applicationId;
    application.firmwareVersion = firmwareVersion;
    application.configurationSchema = configurationSchema;
    application.migration = migration;
    return true;
}
