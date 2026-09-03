#include <Arduino.h>
#include <DeviceFramework.h>
#include "FirmwareIdentity.h"

void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup([] {
        auto& parameters = DeviceFramework::getParameterRegistry();
        parameters.setDefaultValue(
            DeviceFrameworkParameters::PARAM_DEVICE_NAME, "Portal First Example"
        );
    });
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
}
