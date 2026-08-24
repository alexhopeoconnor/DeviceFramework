#ifndef DEVICEFRAMEWORK_RTC_H
#define DEVICEFRAMEWORK_RTC_H

#include "../Configuration/DeviceFrameworkConfig.h" // For RtcData struct
#include <DeviceFrameworkPlatform.h> // Platform detection

/**
 * Platform-agnostic RTC memory abstraction layer
 *
 * ESP8266: Uses system_rtc_mem_read/write with fixed memory addresses
 * ESP32: Uses Preferences API (NVS) for persistent storage
 */
class DeviceFrameworkRTC {
public:
    /**
     * Read RTC data from persistent storage
     * @param data Pointer to RtcData structure to fill
     * @return true if data was successfully read, false otherwise
     */
    static bool read(RtcData* data);

    /**
     * Write RTC data to persistent storage
     * @param data Pointer to RtcData structure to write
     * @return true if data was successfully written, false otherwise
     */
    static bool write(const RtcData* data);

    /**
     * Clear RTC data (set to zero)
     */
    static void clear();

    /**
     * Initialize RTC storage system
     * Must be called before using read/write operations
     */
    static void begin();

private:
    #ifdef DF_PLATFORM_ESP8266
        // ESP8266 uses system_rtc_mem_read/write - no initialization needed
        // Uses CONFIG_rtcMemAddr from DeviceFrameworkConfig
    #elif defined(DF_PLATFORM_ESP32)
        // ESP32 uses Preferences API
        static bool initialized;
        static const char* PREFERENCES_NAMESPACE;
        static const char* RTC_DATA_KEY;
    #endif
};

#endif // DEVICEFRAMEWORK_RTC_H
