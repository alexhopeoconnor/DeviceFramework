#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <MQTT/DeviceFrameworkMQTT.h>

void test_parameter_registry_integration() {
    Serial.println("[TEST]   Testing ParameterRegistry integration...");

    DeviceFrameworkParameterRegistry& registry = DeviceFramework::getParameterRegistry();

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
