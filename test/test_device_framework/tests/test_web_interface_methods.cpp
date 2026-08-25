#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <WiFiClient.h>

// Test WebInterface methods
void test_web_interface_methods() {
    // Test WebInterface enabled status - should be true since it's running in test mode
    bool enabled = DeviceFrameworkWeb::isEnabled();
    TEST_ASSERT_TRUE_MESSAGE(enabled,
        "WebInterface should be enabled in test mode");

    // Test config mode status
    bool configMode = DeviceFrameworkWeb::isInConfigMode();

    // WebInterface config mode depends on WiFi connection status
    bool wifiConnected = WiFi.isConnected();
    if (wifiConnected) {
        TEST_ASSERT_FALSE_MESSAGE(configMode,
            "WebInterface should not be in config mode when WiFi connects");
    } else {
        TEST_ASSERT_TRUE_MESSAGE(configMode,
            "WebInterface should be in config mode when WiFi fails");
    }

    // Test restart doesn't crash
    DeviceFrameworkWeb::restart();
    // Async listener rebinding completes on the network scheduler.
    delay(1000);

    // Test methods still work after restart
    bool enabledAfter = DeviceFrameworkWeb::isEnabled();
    bool configModeAfter = DeviceFrameworkWeb::isInConfigMode();

    // After restart, WebInterface should still be enabled
    TEST_ASSERT_TRUE_MESSAGE(enabledAfter,
        "WebInterface should still be enabled after restart");
    // Config mode should match WiFi connection status
    if (wifiConnected) {
        TEST_ASSERT_FALSE_MESSAGE(configModeAfter,
            "WebInterface should not be in config mode when WiFi connects");
    } else {
        TEST_ASSERT_TRUE_MESSAGE(configModeAfter,
            "WebInterface should be in config mode when WiFi fails");
    }
}
