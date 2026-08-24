#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ArduinoOTA.h>
#include <Arduino.h>
#include "../DeviceFrameworkDebug.h"

class DeviceFrameworkOTA {
public:
    static void setup();
    static void loop();
};

#endif // OTA_MANAGER_H
