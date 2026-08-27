#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <WiFiClient.h>

// Test WiFiManagerModule methods
void test_wifi_manager_state() {
    Serial.println("[TEST]   Testing WiFiManager state...");

    TEST_ASSERT_TRUE_MESSAGE(WiFi.isConnected(),
        "WiFi should be connected after waiting for connection");

    TEST_ASSERT_FALSE_MESSAGE(DeviceFrameworkWiFi::isInConfigMode(),
        "WiFiManager should not be in config mode when WiFi connects");

    TEST_ASSERT_FALSE_MESSAGE(DeviceFrameworkWiFi::getConfigAttempted(),
        "WiFiManager should not have attempted config when WiFi connects successfully");

    WiFiManager& wifiManagerFromWiFiModule = DeviceFrameworkWiFi::getWiFiManager();
    WiFiManager& wifiManagerFromFramework = DeviceFramework::getWiFiManager();

    TEST_ASSERT_EQUAL_PTR_MESSAGE(&wifiManagerFromFramework, &wifiManagerFromWiFiModule,
        "DeviceFramework should expose the same WiFiManager instance as the WiFi module");

    TEST_ASSERT_FALSE_MESSAGE(wifiManagerFromWiFiModule.getConfigPortalActive(),
        "Config portal should not be active during normal operation");

    const WiFiManagerStationProfiles& profiles = wifiManagerFromWiFiModule.getStationProfiles();
    TEST_ASSERT_TRUE_MESSAGE(profiles.slots[0].enabled,
        "The primary WiFi profile should be persisted after a successful candidate connection");
#if defined(TEST_EXPECT_WIFI_FALLBACK) && TEST_EXPECT_WIFI_FALLBACK
    const WiFiManager::wm_station_status_t& station = wifiManagerFromWiFiModule.getStationStatus();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, station.activeSlot,
        "The profile controller should connect through the configured fallback slot");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, profiles.lastSuccessfulSlot,
        "The fallback slot should be remembered for the next connection cycle");
#endif
}
