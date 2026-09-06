#include <Arduino.h>
#include <ArduinoHA.h>
#include <Configuration/DeviceFrameworkParameterMetadata.h>
#include <DeviceFramework.h>
#include <unity.h>

#include "test_config.h"

namespace {

constexpr unsigned long kConnectionTimeoutMs = 120000UL;

HASensorNumber e2eSensor("e2e_sensor", HASensorNumber::PrecisionP0);
HASwitch e2eSwitch("e2e_switch");
bool initialStatePublished = false;
bool suiteComplete = false;
unsigned long connectionWaitStartedAt = 0;

void onE2ESwitchCommand(bool state, HASwitch* sender) {
    sender->setState(state);
    Serial.printf("[HA_E2E] command entity=e2e_switch state=%s\n", state ? "on" : "off");
}

DeviceFrameworkParameterMetadata parameter(
    const char* id,
    const char* label,
    const char* value,
    HAConfigDeviceType type,
    uint16_t order
) {
    DeviceFrameworkParameterMetadata metadata;
    metadata.id = id;
    metadata.label = label;
    metadata.sources = SOURCE_HOME_ASSISTANT;
    metadata.defaultValue = value;
    metadata.order = order;
    metadata.haDeviceType = type;
    return metadata;
}

void registerHAE2EParameters() {
    DeviceFrameworkParameterMetadata number =
        parameter("e2enumber", "E2E Number", "7", HAConfigDeviceType::NUMBER, 200);
    number.valueType = DeviceFrameworkParameterValueType::Float;
    number.hasNumericRange = true;
    number.minValue = 0;
    number.maxValue = 42;
    number.haConstraints.minValue = 0;
    number.haConstraints.maxValue = 42;
    number.haConstraints.step = 1;
    number.haConstraints.precision = 0;
    DeviceFramework::getParameterRegistry().registerParameter(number);

    DeviceFrameworkParameterMetadata select =
        parameter("e2eselect", "E2E Select", "normal", HAConfigDeviceType::SELECT, 201);
    select.valueType = DeviceFrameworkParameterValueType::Enum;
    select.allowedValues = "quiet;normal;verbose";
    select.haConstraints.options = select.allowedValues;
    DeviceFramework::getParameterRegistry().registerParameter(select);

    DeviceFrameworkParameterMetadata text =
        parameter("e2etext", "E2E Text", "ready", HAConfigDeviceType::TEXT, 202);
    text.maxLength = 32;
    text.haConstraints.textMaxLength = text.maxLength;
    DeviceFramework::getParameterRegistry().registerParameter(text);
}

void test_e2e_entity_registration() {
    HAMqtt& mqtt = DeviceFramework::getHAMqtt();
    // The fixture has five target entities plus DeviceFramework's built-in
    // Home Assistant log-level select entity.
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(6, mqtt.getRegisteredDeviceTypeCount(),
        "The compact fixture should register all E2E entity types");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, mqtt.getDeviceTypeRegistrationFailures(),
        "E2E entity registration should not exhaust ArduinoHA capacity");
}

void printReadyMarker() {
    HADevice& device = DeviceFramework::getHADevice();
    Serial.printf("[HA_E2E] ready device_id=%s ip=%s\n",
        device.getUniqueId(), WiFi.localIP().toString().c_str());
    Serial.printf("HA_E2E_DEVICE_ID=%s\n", device.getUniqueId());
    Serial.printf("HA_E2E_DEVICE_IP=%s\n", WiFi.localIP().toString().c_str());
}

void runSuite() {
    UNITY_BEGIN();
    RUN_TEST(test_e2e_entity_registration);
    printReadyMarker();
    UNITY_END();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(5000);
    Serial.println("[HA_E2E] compact DeviceFramework HA/MQTT fixture starting");

    DeviceFramework::beforeSetup([]() { registerHAE2EParameters(); });
    DeviceFrameworkWiFi::preloadWiFi(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
    DeviceFramework::setup();

    DeviceFramework::setDeviceName("DeviceFramework HA E2E");
    DeviceFramework::setMqttServer(TEST_MQTT_SERVER);
    DeviceFramework::setMqttPort(1883);
    DeviceFramework::setMqttUser(TEST_MQTT_USER);
    DeviceFramework::setMqttPass(TEST_MQTT_PASSWORD);

    e2eSensor.setName("E2E Sensor");
    e2eSensor.setUnitOfMeasurement("count");
    e2eSwitch.setName("E2E Switch");
    e2eSwitch.onCommand(onE2ESwitchCommand);
    connectionWaitStartedAt = millis();
}

void loop() {
    DeviceFramework::loop();
    if (suiteComplete) {
        return;
    }

    if (WiFi.isConnected() && DeviceFrameworkMQTT::isConnected()) {
        if (!initialStatePublished) {
            e2eSensor.setValue(1);
            e2eSwitch.setState(false);
            initialStatePublished = true;
        }
        runSuite();
        suiteComplete = true;
        return;
    }

    if (millis() - connectionWaitStartedAt >= kConnectionTimeoutMs) {
        UNITY_BEGIN();
        TEST_FAIL_MESSAGE("Wi-Fi and MQTT did not become ready within the E2E timeout");
        UNITY_END();
        suiteComplete = true;
    }
}
