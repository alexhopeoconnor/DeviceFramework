#include <Arduino.h>
#include <DeviceFramework.h>

namespace {
constexpr const char* APPLICATION_ID = "deviceframework-compile-check";
constexpr const char* FIRMWARE_VERSION = "0.0.0-test";
constexpr uint16_t CONFIGURATION_SCHEMA = 1;
}

void setup() {
    DeviceFramework::configureApplication(APPLICATION_ID, FIRMWARE_VERSION, CONFIGURATION_SCHEMA);
    DeviceFramework::beforeSetup();
}

void loop() {}
