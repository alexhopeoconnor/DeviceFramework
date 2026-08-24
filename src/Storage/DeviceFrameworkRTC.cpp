#include "DeviceFrameworkRTC.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include <DeviceFrameworkPlatform.h>
#include "../DeviceFrameworkDebug.h"
#include <string.h>  // For memset

#ifdef DF_PLATFORM_ESP8266
    // ESP8266 uses system RTC memory
    extern "C" {
        #include "user_interface.h"
    }
#elif defined(DF_PLATFORM_ESP32)
    // ESP32 uses Preferences API
    #include <Preferences.h>

    bool DeviceFrameworkRTC::initialized = false;
    const char* DeviceFrameworkRTC::PREFERENCES_NAMESPACE = "DeviceFramework";
    const char* DeviceFrameworkRTC::RTC_DATA_KEY = "rtc_data";
#endif

void DeviceFrameworkRTC::begin() {
    #ifdef DF_PLATFORM_ESP32
        if (!initialized) {
            Preferences preferences;
            if (preferences.begin(PREFERENCES_NAMESPACE, false)) {
                initialized = true;
                LOG_DEBUGLN(F("DeviceFrameworkRTC: Preferences initialized"));
            } else {
                LOG_ERRORLN(F("DeviceFrameworkRTC: Failed to initialize Preferences"));
            }
        }
    #endif
    // ESP8266 doesn't need initialization - system RTC memory is always available
}

bool DeviceFrameworkRTC::read(RtcData* data) {
    if (!data) {
        LOG_ERRORLN(F("DeviceFrameworkRTC::read: data pointer is null"));
        return false;
    }

    #ifdef DF_PLATFORM_ESP8266
        // ESP8266: Use system RTC memory
        system_rtc_mem_read(CONFIG_rtcMemAddr, data, sizeof(RtcData));
        return true;

    #elif defined(DF_PLATFORM_ESP32)
        // ESP32: Use Preferences API
        if (!initialized) {
            begin();
        }

        Preferences preferences;
        if (!preferences.begin(PREFERENCES_NAMESPACE, true)) { // Read-only mode
            LOG_ERRORLN(F("DeviceFrameworkRTC::read: Failed to open Preferences"));
            return false;
        }

        size_t dataSize = preferences.getBytes(RTC_DATA_KEY, data, sizeof(RtcData));
        preferences.end();

        if (dataSize != sizeof(RtcData)) {
            // No data stored yet or wrong size
            LOG_DEBUGLN(F("DeviceFrameworkRTC::read: No valid data found in Preferences"));
            return false;
        }

        return true;
    #endif
}

bool DeviceFrameworkRTC::write(const RtcData* data) {
    if (!data) {
        LOG_ERRORLN(F("DeviceFrameworkRTC::write: data pointer is null"));
        return false;
    }

    #ifdef DF_PLATFORM_ESP8266
        // ESP8266: Use system RTC memory
        system_rtc_mem_write(CONFIG_rtcMemAddr, data, sizeof(RtcData));
        return true;

    #elif defined(DF_PLATFORM_ESP32)
        // ESP32: Use Preferences API
        if (!initialized) {
            begin();
        }

        Preferences preferences;
        if (!preferences.begin(PREFERENCES_NAMESPACE, false)) { // Read-write mode
            LOG_ERRORLN(F("DeviceFrameworkRTC::write: Failed to open Preferences"));
            return false;
        }

        size_t written = preferences.putBytes(RTC_DATA_KEY, data, sizeof(RtcData));
        preferences.end();

        if (written != sizeof(RtcData)) {
            LOG_ERRORLN(F("DeviceFrameworkRTC::write: Failed to write data to Preferences"));
            return false;
        }

        return true;
    #endif
}

void DeviceFrameworkRTC::clear() {
    RtcData emptyData = {};
    memset(&emptyData, 0, sizeof(RtcData));
    write(&emptyData);
}
