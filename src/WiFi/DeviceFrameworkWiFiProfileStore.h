#ifndef DEVICEFRAMEWORK_WIFI_PROFILE_STORE_H
#define DEVICEFRAMEWORK_WIFI_PROFILE_STORE_H

#include <WiFiManager.h>

class DeviceFrameworkWiFiProfileStore final : public WiFiManagerStationProfileStore {
public:
    bool load(WiFiManagerStationProfiles& profiles) override;
    bool save(const WiFiManagerStationProfiles& profiles) override;
    bool clear() override;
};

#endif // DEVICEFRAMEWORK_WIFI_PROFILE_STORE_H
