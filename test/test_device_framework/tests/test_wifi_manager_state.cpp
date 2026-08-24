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
}
