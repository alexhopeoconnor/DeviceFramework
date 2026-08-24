#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <ArduinoHA.h>
#include "test_main.h"

// Test MQTT settings and functionality (third test - after parameter setup)
void test_device_framework_mqtt_settings() {
    // Test MQTT connection status - should be connected since we wait for it
    bool mqttConnected = DeviceFrameworkMQTT::isConnected();
    TEST_ASSERT_TRUE_MESSAGE(mqttConnected,
        "MQTT should be connected after waiting for connection");

    // Test HADevice configuration
    HADevice& haDevice = DeviceFrameworkMQTT::getHADevice();

    // Test HADevice unique ID is set (should be MAC address)
    const char* uniqueId = haDevice.getUniqueId();
    TEST_ASSERT_NOT_NULL_MESSAGE(uniqueId,
        "HADevice unique ID should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strlen(uniqueId) > 0,
        "HADevice unique ID should not be empty");

    // Test HADevice availability status
    bool isAvailable = haDevice.isAvailable();
    TEST_ASSERT_TRUE_MESSAGE(isAvailable,
        "HADevice should be available (online)");

    // Test HADevice shared availability is enabled
    bool sharedAvailability = haDevice.isSharedAvailabilityEnabled();
    TEST_ASSERT_TRUE_MESSAGE(sharedAvailability,
        "HADevice shared availability should be enabled");

    // Test HADevice extended unique IDs is enabled
    bool extendedUniqueIds = haDevice.isExtendedUniqueIdsEnabled();
    TEST_ASSERT_TRUE_MESSAGE(extendedUniqueIds,
        "HADevice extended unique IDs should be enabled");

    // Test HADevice availability topic is generated
    const char* availabilityTopic = haDevice.getAvailabilityTopic();
    TEST_ASSERT_NOT_NULL_MESSAGE(availabilityTopic,
        "HADevice availability topic should not be NULL");
    TEST_ASSERT_TRUE_MESSAGE(strlen(availabilityTopic) > 0,
        "HADevice availability topic should not be empty");

    // Test topic generation functions
    String sharedTopic = DeviceFrameworkMQTT::generateSharedTopic("test_command");
    TEST_ASSERT_FALSE_MESSAGE(sharedTopic.isEmpty(),
        "generateSharedTopic should return non-empty topic");
    TEST_ASSERT_TRUE_MESSAGE(sharedTopic.startsWith("aha/"),
        "generateSharedTopic should start with 'aha/'");
    TEST_ASSERT_TRUE_MESSAGE(sharedTopic.endsWith("/test_command"),
        "generateSharedTopic should end with '/test_command'");

    // Test device-specific topic generation with our test entities
    String deviceTopic = DeviceFrameworkMQTT::generateDeviceSpecificTopic(&testSensor, "command");
    TEST_ASSERT_FALSE_MESSAGE(deviceTopic.isEmpty(),
        "generateDeviceSpecificTopic should return non-empty topic");
    TEST_ASSERT_TRUE_MESSAGE(deviceTopic.indexOf("test_sensor") >= 0,
        "generateDeviceSpecificTopic should contain device unique ID");
    TEST_ASSERT_TRUE_MESSAGE(deviceTopic.endsWith("/command"),
        "generateDeviceSpecificTopic should end with '/command'");

    // Test MQTT command functionality (MQTT should be connected)
    Serial.println("[TEST]     Testing MQTT command functionality...");

    // Test that we can access the MQTT client
    HAMqtt& mqttClient = DeviceFrameworkMQTT::getHAMqtt();

    // Test publishing to shared reset command topic
    String resetTopic = DeviceFrameworkMQTT::generateSharedTopic("reset_command");
    TEST_ASSERT_FALSE_MESSAGE(resetTopic.isEmpty(),
        "Reset command topic should be generated");

    // Test publishing a test message (this will actually publish to MQTT)
    bool publishResult = mqttClient.publish(resetTopic.c_str(), "test", false);
    TEST_ASSERT_TRUE_MESSAGE(publishResult,
        "Should be able to publish test message to MQTT");

    Serial.println("[TEST]     MQTT test message published successfully");
}
