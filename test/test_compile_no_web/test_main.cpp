#include <Arduino.h>
#include <DeviceFramework.h>
#include <DeviceFrameworkDebug.h>

void setup() {
    LOG_INFOLN(F("Compile-only web-disabled configuration"));
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
}
