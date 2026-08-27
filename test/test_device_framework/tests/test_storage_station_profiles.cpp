#include <unity.h>

#include <Storage/DeviceFrameworkStorage.h>

namespace {
void setProfile(WiFiManagerStationProfile& profile, const char* ssid, const char* password) {
    profile.enabled = true;
    profile.hasPassword = password && password[0] != 0;
    strncpy(profile.ssid, ssid, sizeof(profile.ssid) - 1);
    if (profile.hasPassword) strncpy(profile.password, password, sizeof(profile.password) - 1);
}
}

void test_storage_station_profiles_round_trip() {
    const WiFiManagerStationProfiles original = DeviceFrameworkStorage::getStationProfiles();

    WiFiManagerStationProfiles profiles;
    setProfile(profiles.slots[0], "PrimaryNetwork", "primary-password");
    setProfile(profiles.slots[1], "FallbackNetwork", "fallback-password");
    profiles.preferredSlot = 0;
    profiles.lastSuccessfulSlot = 1;

    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFrameworkStorage::saveWithStationProfiles(profiles),
        "Two WiFi profiles should save in one transactional record"
    );

    DeviceFrameworkStorage::setStationProfiles(WiFiManagerStationProfiles());
    const DeviceFrameworkStorageLoadResult loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(), "Profile record should reload");

    const WiFiManagerStationProfiles& restored = DeviceFrameworkStorage::getStationProfiles();
    TEST_ASSERT_TRUE(restored.slots[0].enabled);
    TEST_ASSERT_TRUE(restored.slots[0].hasPassword);
    TEST_ASSERT_EQUAL_STRING("PrimaryNetwork", restored.slots[0].ssid);
    TEST_ASSERT_EQUAL_STRING("primary-password", restored.slots[0].password);
    TEST_ASSERT_TRUE(restored.slots[1].enabled);
    TEST_ASSERT_TRUE(restored.slots[1].hasPassword);
    TEST_ASSERT_EQUAL_STRING("FallbackNetwork", restored.slots[1].ssid);
    TEST_ASSERT_EQUAL_STRING("fallback-password", restored.slots[1].password);
    TEST_ASSERT_EQUAL_UINT8(0, restored.preferredSlot);
    TEST_ASSERT_EQUAL_UINT8(1, restored.lastSuccessfulSlot);

    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFrameworkStorage::saveWithStationProfiles(original),
        "The test's prior profile set should restore"
    );
}
