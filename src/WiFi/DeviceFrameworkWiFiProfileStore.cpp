#include "DeviceFrameworkWiFiProfileStore.h"

#include "../Storage/DeviceFrameworkStorage.h"

bool DeviceFrameworkWiFiProfileStore::load(WiFiManagerStationProfiles& profiles) {
    profiles = DeviceFrameworkStorage::getStationProfiles();
    return true;
}

bool DeviceFrameworkWiFiProfileStore::save(const WiFiManagerStationProfiles& profiles) {
    return DeviceFrameworkStorage::saveWithStationProfiles(profiles);
}

bool DeviceFrameworkWiFiProfileStore::clear() {
    return DeviceFrameworkStorage::saveWithStationProfiles(WiFiManagerStationProfiles());
}
