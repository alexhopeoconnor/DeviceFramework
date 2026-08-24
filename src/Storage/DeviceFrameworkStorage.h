#ifndef DEVICEFRAMEWORK_STORAGE_H
#define DEVICEFRAMEWORK_STORAGE_H

#include <EEPROM.h>
#include <map>
#include <Arduino.h>
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../DeviceFrameworkDebug.h"

class DeviceFrameworkStorage {
public:
    static void setup();
    static void save();
    static void load();
    static void clear();
    static bool isVersionValid();
};

#endif // DEVICEFRAMEWORK_STORAGE_H
