#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <MQTT/DeviceFrameworkMQTT.h>

void test_parameter_registry_integration() {
    Serial.println("[TEST]   Testing ParameterRegistry integration...");

    DeviceFrameworkParameterRegistry& registry = DeviceFramework::getParameterRegistry();

    TEST_ASSERT_EQUAL_UINT_MESSAGE(10, registry.getParameterCount(),
        "Six core plus four custom fixture parameters should be registered");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(10, registry.getAllocatedCapacity(),
        "The four-parameter allocation hint should reserve exactly ten entries");

    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_DEVICE_NAME),
        "ParameterRegistry should register the core device name parameter");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_SERVER),
        "ParameterRegistry should register the core MQTT server parameter");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_PORT),
        "ParameterRegistry should register the core MQTT port parameter");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_USER),
        "ParameterRegistry should register the core MQTT user parameter");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(DeviceFrameworkParameters::PARAM_MQTT_PASS),
        "ParameterRegistry should register the core MQTT password parameter");

    String registryDeviceName = registry.getValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(DeviceFramework::getDeviceName(), registryDeviceName.c_str(),
        "Registry value for device name should match DeviceFramework accessor");

    String registryMqttServer = registry.getValue(DeviceFrameworkParameters::PARAM_MQTT_SERVER);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(DeviceFramework::getMqttServer(), registryMqttServer.c_str(),
        "Registry value for MQTT server should match DeviceFramework accessor");

    uint16_t registryMqttPort = registry.getValueAsInt(DeviceFrameworkParameters::PARAM_MQTT_PORT);
    TEST_ASSERT_EQUAL_MESSAGE(DeviceFramework::getMqttPort(), registryMqttPort,
        "Registry value for MQTT port should match DeviceFramework accessor");

    String registryMqttUser = registry.getValue(DeviceFrameworkParameters::PARAM_MQTT_USER);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(DeviceFramework::getMqttUser(), registryMqttUser.c_str(),
        "Registry value for MQTT user should match DeviceFramework accessor");

    String registryMqttPass = registry.getValue(DeviceFrameworkParameters::PARAM_MQTT_PASS);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(DeviceFramework::getMqttPass(), registryMqttPass.c_str(),
        "Registry value for MQTT password should match DeviceFramework accessor");

    const char* customParameterId = "testdevicename";
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(customParameterId),
        "Custom WiFiManager parameter should be registered in the ParameterRegistry");

    const char* customParameterValue = DeviceFramework::getCustomParameterValue(customParameterId);
    TEST_ASSERT_NOT_NULL_MESSAGE(customParameterValue,
        "Custom parameter value should be retrievable via DeviceFramework");

    String registryCustomValue = registry.getValue(customParameterId);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(customParameterValue, registryCustomValue.c_str(),
        "Custom parameter value should match between DeviceFramework and registry");

    WiFiManagerParameter* customWiFiParam = registry.getWiFiManagerParameter(customParameterId);
    TEST_ASSERT_NOT_NULL_MESSAGE(customWiFiParam,
        "ParameterRegistry should expose the WiFiManagerParameter instance for custom parameters");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(registryCustomValue.c_str(), customWiFiParam->getValue(),
        "WiFiManagerParameter value should reflect the registry value");

    // DeviceFramework releases its temporary pointer array after setup. The
    // parameter objects themselves must remain owned by the registry and be
    // retained by WiFiManager for the lifetime of the portal.
    WiFiManager& wifiManager = DeviceFramework::getWiFiManager();
    ParameterIdList portalParameterIds =
        registry.getParameterIdsSorted(DeviceFrameworkParameterSource::SOURCE_WIFI_MANAGER);
    const size_t wifiManagerParameterCount =
        static_cast<size_t>(wifiManager.getParametersCount());
    TEST_ASSERT_EQUAL_UINT_MESSAGE(
        static_cast<unsigned int>(portalParameterIds.count),
        static_cast<unsigned int>(wifiManagerParameterCount),
        "WiFiManager should retain every registry portal parameter"
    );

    WiFiManagerParameter** portalParameters = wifiManager.getParameters();
    if (portalParameterIds.count > 0) {
        TEST_ASSERT_NOT_NULL_MESSAGE(portalParameters,
            "WiFiManager should retain a pointer list when portal parameters exist");
    }

    for (size_t i = 0; i < portalParameterIds.count; ++i) {
        WiFiManagerParameter* registryParameter =
            registry.getWiFiManagerParameter(portalParameterIds.ids[i]);
        TEST_ASSERT_NOT_NULL_MESSAGE(registryParameter,
            "Each registry portal parameter should have a WiFiManager instance");

        bool foundInWiFiManager = false;
        for (size_t parameterIndex = 0;
             parameterIndex < wifiManagerParameterCount;
             ++parameterIndex) {
            if (portalParameters[parameterIndex] == registryParameter) {
                foundInWiFiManager = true;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(foundInWiFiManager,
            "WiFiManager should retain the registry-owned parameter instance");
    }

    // Verify that updates propagate to the registry and WiFiManager parameter
    String originalCustomValue = registryCustomValue;
    const char* updatedCustomValue = "split-test";

    DeviceFramework::setCustomParameterValue(customParameterId, updatedCustomValue);

    String registryUpdatedValue = registry.getValue(customParameterId);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(updatedCustomValue, registryUpdatedValue.c_str(),
        "Setting a custom parameter should update the registry value");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(updatedCustomValue, customWiFiParam->getValue(),
        "WiFiManagerParameter should stay in sync after updating a custom parameter");

    DeviceFramework::setCustomParameterValue(customParameterId, originalCustomValue.c_str());
    String registryRestoredValue = registry.getValue(customParameterId);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(originalCustomValue.c_str(), registryRestoredValue.c_str(),
        "Restoring the custom parameter should restore the registry value");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(originalCustomValue.c_str(), customWiFiParam->getValue(),
        "WiFiManagerParameter should reflect the restored custom parameter value");
}

void test_parameter_registry_const_char_read_access() {
    Serial.println("[TEST]   Testing const-char ParameterRegistry reads...");

    constexpr char kLongParameterId[] = "testdevicename";
    constexpr char kUnknownParameterId[] = "unknownparameter";
    DeviceFrameworkParameterRegistry& registry = DeviceFramework::getParameterRegistry();
    const String originalValue = registry.getValue(kLongParameterId);

    TEST_ASSERT_TRUE_MESSAGE(registry.setValue(String(kLongParameterId), "12345"),
        "Long-ID test parameter should accept a numeric value");
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter(kLongParameterId),
        "const-char lookup should find a long parameter ID");
    TEST_ASSERT_NOT_NULL_MESSAGE(registry.getMetadata(kLongParameterId),
        "const-char metadata lookup should find a long parameter ID");
    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, registry.getValueAsInt(kLongParameterId),
        "const-char integer reads should parse the stored value directly");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 12345.0f,
        registry.getValueAsFloat(kLongParameterId),
        "const-char float reads should parse the stored value directly");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("12345", registry.getValueAsCStr(kLongParameterId),
        "const-char C-string reads should borrow the stored value");

    const String stringId(kLongParameterId);
    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, registry.getValueAsInt(stringId),
        "String-ID integer reads should retain the direct stored-value path");

    TEST_ASSERT_TRUE_MESSAGE(registry.setValue(String(kLongParameterId), "true"),
        "Long-ID test parameter should accept a boolean value");
    TEST_ASSERT_TRUE_MESSAGE(registry.getValueAsBool(kLongParameterId),
        "const-char boolean reads should parse the stored value directly");

    TEST_ASSERT_FALSE_MESSAGE(registry.hasParameter(kUnknownParameterId),
        "Unknown const-char IDs should not appear registered");
    TEST_ASSERT_NULL_MESSAGE(registry.getMetadata(kUnknownParameterId),
        "Unknown const-char IDs should have no metadata");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, registry.getValueAsInt(kUnknownParameterId),
        "Unknown const-char integer reads should retain the zero fallback");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 0.0f,
        registry.getValueAsFloat(kUnknownParameterId),
        "Unknown const-char float reads should retain the zero fallback");
    TEST_ASSERT_FALSE_MESSAGE(registry.getValueAsBool(kUnknownParameterId),
        "Unknown const-char boolean reads should retain the false fallback");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", registry.getValueAsCStr(kUnknownParameterId),
        "Unknown const-char C-string reads should retain the empty fallback");

    TEST_ASSERT_FALSE_MESSAGE(registry.hasParameter(nullptr),
        "Null IDs should be handled without a lookup crash");
    TEST_ASSERT_NULL_MESSAGE(registry.getMetadata(nullptr),
        "Null IDs should have no metadata");

    TEST_ASSERT_TRUE_MESSAGE(registry.setValue(String(kLongParameterId), originalValue),
        "Const-char read test should restore its fixture value");
}

void test_parameter_registry_ha_origin_updates_shadow_state() {
    Serial.println("[TEST]   Testing HA-origin shadow state updates...");

    DeviceFrameworkParameterRegistry& registry = DeviceFramework::getParameterRegistry();
    TEST_ASSERT_TRUE_MESSAGE(registry.hasParameter("testhaswitch"),
        "Custom HA switch parameter should be registered");

    HASwitch* haSwitch = static_cast<HASwitch*>(registry.getHADeviceForParameter("testhaswitch"));
    TEST_ASSERT_NOT_NULL_MESSAGE(haSwitch,
        "ParameterRegistry should expose the HA switch instance for the custom HA parameter");

    // Force a disconnected state so a callback-local publish would fail.
    HAMqtt& mqttClient = DeviceFrameworkMQTT::getHAMqtt();
    mqttClient.disconnect();
    registry.setMqttReady(false);

    registry.setValue("testhaswitch", false);
    haSwitch->setCurrentState(false);

    DeviceFrameworkParameterRegistry::onHASwitchCommand(true, haSwitch);

    TEST_ASSERT_TRUE_MESSAGE(registry.getValueAsBool("testhaswitch"),
        "HA-originated writes should update the registry value");
    TEST_ASSERT_TRUE_MESSAGE(haSwitch->getCurrentState(),
        "HA-originated writes should update the local HA shadow state without requiring a publish");
}

void test_parameter_registry_capacity_hint_fallback() {
    Serial.println("[TEST]   Testing ParameterRegistry capacity-hint fallback...");

    DeviceFrameworkParameterRegistry& registry = DeviceFramework::getParameterRegistry();
    TEST_ASSERT_TRUE_MESSAGE(registry.ensureParameterCapacity(11),
        "An underestimated capacity hint should still allow normal registry growth");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(16, registry.getAllocatedCapacity(),
        "An exact ten-entry hint should grow to the canonical sixteen-entry tier");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(10, registry.getParameterCount(),
        "Growing capacity must preserve existing registered entries");
}
