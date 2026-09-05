#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <OTA/DeviceFrameworkOTA.h>
#include <MDNS/DeviceFrameworkMDNS.h>
#include <Storage/DeviceFrameworkStorage.h>
#include <Provisioning/DeviceFrameworkProvisioning.h>
#include <ArduinoOTA.h>
#include <Configuration/DeviceFrameworkParameterRegistry.h>
#include <Utils/HostnameUtils.h>

#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
#include <DeviceFrameworkLocalProfile.h>
#endif

// Test DeviceFramework setup verification - verify all subsystems initialized correctly
void test_device_framework_setup_verification() {
    Serial.println("[TEST]   Testing DeviceFramework setup verification...");

    // Verify OTA was initialized during DeviceFramework::setup()
    String otaHostname = ArduinoOTA.getHostname();
    TEST_ASSERT_FALSE_MESSAGE(otaHostname.isEmpty(),
        "OTA hostname should be set after DeviceFramework::setup()");

    // OTA advertises the device name that existed while setup initialized it.
    // A later runtime display-name edit is persisted for the next boot; it does
    // not rename mDNS/OTA in place. Profiles may supply that startup name.
#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
    const char* profileDeviceName = nullptr;
    for (size_t index = 0; index < DEVICEFRAMEWORK_PROFILE_PARAMETER_COUNT; ++index) {
        const DeviceFrameworkProvisionedParameter& parameter = DEVICEFRAMEWORK_PROFILE_PARAMETERS[index];
        if (strcmp(parameter.id, "device") == 0) {
            profileDeviceName = parameter.value;
            break;
        }
    }
    if (profileDeviceName) {
        const String expectedHostname = HostnameUtils::sanitizeHostname(profileDeviceName);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedHostname.c_str(), otaHostname.c_str(),
            "OTA hostname should use the profile device name selected at setup");
    } else
#endif
    {
        const char* sanitizedHostname = DeviceFramework::getSanitizedHostname();
        TEST_ASSERT_NOT_NULL_MESSAGE(sanitizedHostname,
            "Sanitized hostname should not be NULL");
        TEST_ASSERT_EQUAL_STRING_MESSAGE(sanitizedHostname, otaHostname.c_str(),
            "OTA hostname should match DeviceFramework sanitized hostname");
    }

    // mDNS starts only after a stable usable network and adequate heap. A fixed
    // numeric endpoint must still work while that optional responder is deferred.
    IPAddress literalAddress;
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkMDNS::resolveCached("192.0.2.1", literalAddress),
        "A numeric endpoint should not depend on mDNS initialization");
    TEST_ASSERT_EQUAL_UINT8(192, literalAddress[0]);
    TEST_ASSERT_EQUAL_UINT8(0, literalAddress[1]);
    TEST_ASSERT_EQUAL_UINT8(2, literalAddress[2]);
    TEST_ASSERT_EQUAL_UINT8(1, literalAddress[3]);

    const DeviceFrameworkStorageLoadResult storage = DeviceFrameworkStorage::getLastLoadResult();
    TEST_ASSERT_TRUE_MESSAGE(storage.hasUsableConfiguration(), "Storage should load a V4 configuration after setup");

    HAMqtt& mqtt = DeviceFramework::getHAMqtt();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, mqtt.getRegisteredDeviceTypeCount(),
        "ArduinoHA should register the fixture's static entities and HA-backed parameters");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, mqtt.getDeviceTypeRegistrationFailures(),
        "Automatic registration should not hit ArduinoHA's entity limit");


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
