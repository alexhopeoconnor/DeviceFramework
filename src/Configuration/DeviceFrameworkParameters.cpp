#include "DeviceFrameworkParameters.h"
#include "../DeviceFrameworkDebug.h"
#include <DeviceFrameworkPlatform.h>

// Static member initialization
DeviceFrameworkParameterRegistry DeviceFrameworkParameters::registry;

// Core parameter IDs (private constants)
const char* DeviceFrameworkParameters::PARAM_DEVICE_NAME = "device";
const char* DeviceFrameworkParameters::PARAM_MQTT_SERVER = "mqttserver";
const char* DeviceFrameworkParameters::PARAM_MQTT_PORT = "mqttport";
const char* DeviceFrameworkParameters::PARAM_MQTT_USER = "mqttuser";
const char* DeviceFrameworkParameters::PARAM_MQTT_PASS = "mqttpass";
const char* DeviceFrameworkParameters::PARAM_LOG_LEVEL = "loglevel";

void DeviceFrameworkParameters::initialize() {
    registerCoreParameters();
}

void DeviceFrameworkParameters::registerCoreParameters() {
    // Register device name parameter (order 0 - first in EEPROM)
    DeviceFrameworkParameterMetadata deviceNameMeta;
    deviceNameMeta.id = PARAM_DEVICE_NAME;
    deviceNameMeta.label = "Device Name";
    deviceNameMeta.sources = SOURCE_WIFI_MANAGER;
    deviceNameMeta.defaultValue = DF_DEFAULT_DEVICE_NAME;
    deviceNameMeta.maxLength = 24;
    deviceNameMeta.order = 0;
    // HTML attributes for better UX
    deviceNameMeta.htmlAttributes.autocorrect = "off";
    deviceNameMeta.htmlAttributes.autocomplete = "off";
    registry.registerParameter(deviceNameMeta);

    // Register MQTT server parameter (order 1)
    DeviceFrameworkParameterMetadata mqttServerMeta;
    mqttServerMeta.id = PARAM_MQTT_SERVER;
    mqttServerMeta.label = "MQTT Server";
    mqttServerMeta.sources = SOURCE_WIFI_MANAGER;
    mqttServerMeta.defaultValue = "mqtt.local";
    mqttServerMeta.maxLength = 40;
    mqttServerMeta.order = 1;
    // HTML attributes for better UX - prevent autocapitalization on server addresses
    mqttServerMeta.htmlAttributes.autocapitalize = "off";
    mqttServerMeta.htmlAttributes.autocorrect = "off";
    mqttServerMeta.htmlAttributes.autocomplete = "off";
    mqttServerMeta.htmlAttributes.inputmode = "url";
    registry.registerParameter(mqttServerMeta);

    // Register MQTT port parameter (order 2)
    DeviceFrameworkParameterMetadata mqttPortMeta;
    mqttPortMeta.id = PARAM_MQTT_PORT;
    mqttPortMeta.label = "MQTT Port";
    mqttPortMeta.sources = SOURCE_WIFI_MANAGER;
    mqttPortMeta.defaultValue = "1883";
    mqttPortMeta.maxLength = 6;
    mqttPortMeta.order = 2;
    mqttPortMeta.valueType = DeviceFrameworkParameterValueType::UnsignedInteger;
    mqttPortMeta.hasNumericRange = true;
    mqttPortMeta.minValue = 1; mqttPortMeta.maxValue = 65535;
    // HTML attributes for better UX - numeric keyboard on mobile
    mqttPortMeta.htmlAttributes.inputmode = "numeric";
    mqttPortMeta.htmlAttributes.autocomplete = "off";
    registry.registerParameter(mqttPortMeta);

    // Register MQTT user parameter (order 3)
    DeviceFrameworkParameterMetadata mqttUserMeta;
    mqttUserMeta.id = PARAM_MQTT_USER;
    mqttUserMeta.label = "MQTT User";
    mqttUserMeta.sources = SOURCE_WIFI_MANAGER;
    mqttUserMeta.defaultValue = "user";
    mqttUserMeta.maxLength = 20;
    mqttUserMeta.order = 3;
    // HTML attributes for better UX - usernames shouldn't be auto-capitalized
    mqttUserMeta.htmlAttributes.autocapitalize = "off";
    mqttUserMeta.htmlAttributes.autocorrect = "off";
    mqttUserMeta.htmlAttributes.autocomplete = "username";
    registry.registerParameter(mqttUserMeta);

    // Register MQTT password parameter (order 4)
    DeviceFrameworkParameterMetadata mqttPassMeta;
    mqttPassMeta.id = PARAM_MQTT_PASS;
    mqttPassMeta.label = "MQTT Password";
    mqttPassMeta.sources = SOURCE_WIFI_MANAGER;
    mqttPassMeta.defaultValue = "pass";
    mqttPassMeta.maxLength = 20;
    mqttPassMeta.order = 4;
    // HTML attributes for better UX - password field (hidden by default with show toggle)
    mqttPassMeta.htmlAttributes.inputType = "password";
    mqttPassMeta.htmlAttributes.autocapitalize = "off";
    mqttPassMeta.htmlAttributes.autocorrect = "off";
    mqttPassMeta.htmlAttributes.autocomplete = "current-password";
    registry.registerParameter(mqttPassMeta);

    // Register log level parameter (order 5)
    DeviceFrameworkParameterMetadata logLevelMeta;
    logLevelMeta.id = PARAM_LOG_LEVEL;
    logLevelMeta.label = "Log Level";
    logLevelMeta.sources = SOURCE_WIFI_MANAGER | SOURCE_HOME_ASSISTANT;
    // Set default to compile-time LOG_LEVEL converted to string
    logLevelMeta.defaultValue = logLevelToString(DEFAULT_LOG_LEVEL);
    logLevelMeta.maxLength = 8;  // "Verbose" is 7 chars + null terminator
    logLevelMeta.order = 5;
    logLevelMeta.valueType = DeviceFrameworkParameterValueType::Enum;
    logLevelMeta.allowedValues = "Off;Error;Warn;Info;Verbose";

    // Home Assistant configuration - use SELECT for dropdown
    logLevelMeta.haDeviceType = HAConfigDeviceType::SELECT;
    logLevelMeta.haConstraints.options = "Off;Error;Warn;Info;Verbose";  // Semicolon-separated
    logLevelMeta.haEntityCommon.icon = "mdi:text-box-search";
    logLevelMeta.haEntityCommon.entityCategory = "config";

    // HTML attributes for WiFiManager - configure as select element
    logLevelMeta.htmlAttributes.inputType = "select";
    logLevelMeta.htmlAttributes.options = "Off;Error;Warn;Info;Verbose";  // Semicolon-separated
    logLevelMeta.htmlAttributes.autocomplete = "off";

    registry.registerParameter(logLevelMeta);
}

// Generic parameter access
const char* DeviceFrameworkParameters::getValue(const char* id) {
    return registry.getValueAsCStr(id);
}

bool DeviceFrameworkParameters::setValue(const char* id, const char* value) {
    return registry.setValue(id, value);
}

bool DeviceFrameworkParameters::setValue(const char* id, int value) {
    return registry.setValue(id, value);
}

bool DeviceFrameworkParameters::setValue(const char* id, float value, int decimalPlaces) {
    return registry.setValue(id, value, decimalPlaces);
}

bool DeviceFrameworkParameters::setValue(const char* id, bool value) {
    return registry.setValue(id, value);
}

// Core parameter getters
const char* DeviceFrameworkParameters::getDeviceName() {
    return getValue(PARAM_DEVICE_NAME);
}

const char* DeviceFrameworkParameters::getMqttServer() {
    return getValue(PARAM_MQTT_SERVER);
}

uint16_t DeviceFrameworkParameters::getMqttPort() {
    return registry.getValueAsInt(PARAM_MQTT_PORT);
}

const char* DeviceFrameworkParameters::getMqttUser() {
    return getValue(PARAM_MQTT_USER);
}

const char* DeviceFrameworkParameters::getMqttPass() {
    return getValue(PARAM_MQTT_PASS);
}

const char* DeviceFrameworkParameters::getLogLevel() {
    return getValue(PARAM_LOG_LEVEL);
}

int DeviceFrameworkParameters::getLogLevelAsInt() {
    const char* levelStr = getValue(PARAM_LOG_LEVEL);
    LogLevel level = stringToLogLevel(levelStr);
    return static_cast<int>(level);
}

// Core parameter setters
void DeviceFrameworkParameters::setDeviceName(const char* name) {
    setValue(PARAM_DEVICE_NAME, name);
}

void DeviceFrameworkParameters::setMqttServer(const char* server) {
    setValue(PARAM_MQTT_SERVER, server);
}

void DeviceFrameworkParameters::setMqttPort(uint16_t port) {
    setValue(PARAM_MQTT_PORT, (int)port);
}

void DeviceFrameworkParameters::setMqttUser(const char* user) {
    setValue(PARAM_MQTT_USER, user);
}

void DeviceFrameworkParameters::setMqttPass(const char* pass) {
    setValue(PARAM_MQTT_PASS, pass);
}

void DeviceFrameworkParameters::setLogLevel(LogLevel level) {
    const char* levelStr = logLevelToString(level);
    setValue(PARAM_LOG_LEVEL, levelStr);
}

// Parameter management
void DeviceFrameworkParameters::restoreDefaults() {
    auto ids = registry.getParameterIds();
    for (size_t i = 0; i < ids.count; i++) {
        const String& id = ids.ids[i];
        const DeviceFrameworkParameterMetadata* meta = registry.getMetadata(id);
        if (meta) {
            registry.setValue(id, meta->defaultValue);
        }
    }
}

// Access to the underlying registry
DeviceFrameworkParameterRegistry& DeviceFrameworkParameters::getRegistry() {
    return registry;
}
