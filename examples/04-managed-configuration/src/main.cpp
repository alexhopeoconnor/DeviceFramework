#include <Arduino.h>
#include <DeviceFramework.h>
#include "FirmwareIdentity.h"

void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup([] {
        DeviceFramework::getParameterRegistry().setDefaultValue(
            DeviceFrameworkParameters::PARAM_DEVICE_NAME, "Managed configuration example"
        );
    });
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();
}
