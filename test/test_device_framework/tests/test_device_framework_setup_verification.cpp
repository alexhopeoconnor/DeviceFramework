#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <OTA/DeviceFrameworkOTA.h>
#include <MDNS/DeviceFrameworkMDNS.h>
#include <Storage/DeviceFrameworkStorage.h>
#include <ArduinoOTA.h>
#include <Configuration/DeviceFrameworkParameterRegistry.h>

// Test DeviceFramework setup verification - verify all subsystems initialized correctly
void test_device_framework_setup_verification() {
    Serial.println("[TEST]   Testing DeviceFramework setup verification...");

    // Verify OTA was initialized during DeviceFramework::setup()
    String otaHostname = ArduinoOTA.getHostname();
    TEST_ASSERT_FALSE_MESSAGE(otaHostname.isEmpty(),
        "OTA hostname should be set after DeviceFramework::setup()");

    // Verify OTA hostname matches sanitized hostname
    const char* sanitizedHostname = DeviceFramework::getSanitizedHostname();
    TEST_ASSERT_NOT_NULL_MESSAGE(sanitizedHostname,
        "Sanitized hostname should not be NULL");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(sanitizedHostname, otaHostname.c_str(),
        "OTA hostname should match DeviceFramework sanitized hostname");

    // Verify MDNS was initialized during DeviceFramework::setup()
    bool mdnsInitialized = DeviceFrameworkMDNS::isInitialized();
    TEST_ASSERT_TRUE_MESSAGE(mdnsInitialized,
        "MDNS should be initialized after DeviceFramework::setup()");

    const DeviceFrameworkStorageLoadResult storage = DeviceFrameworkStorage::getLastLoadResult();
    TEST_ASSERT_TRUE_MESSAGE(storage.hasUsableConfiguration(), "Storage should load a V4 configuration after setup");


    // Verify ParameterRegistry has core parameters registered
    DeviceFrameworkParameterRegistry& registry = DeviceFramework::getParameterRegistry();
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_DEVICE_NAME),
        "ParameterRegistry should have PARAM_DEVICE_NAME registered");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_SERVER),
        "ParameterRegistry should have PARAM_MQTT_SERVER registered");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_PORT),
        "ParameterRegistry should have PARAM_MQTT_PORT registered");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_USER),
        "ParameterRegistry should have PARAM_MQTT_USER registered");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_PASS),
        "ParameterRegistry should have PARAM_MQTT_PASS registered");

    // Verify custom parameter from beforeSetup() callback was registered
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter("testdevicename"),
        "Custom parameter 'testdevicename' should be registered from beforeSetup() callback");

    Serial.println("[TEST]   DeviceFramework setup verification completed successfully");
}
