#ifndef DEVICEFRAMEWORK_PLATFORM_H
#define DEVICEFRAMEWORK_PLATFORM_H

// Platform detection and abstraction layer
// Supports ESP8266 and ESP32 platforms with board-agnostic design

// Platform detection
#ifdef ESP8266
    #define DF_PLATFORM_ESP8266
#elif defined(ESP32)
    #define DF_PLATFORM_ESP32
#else
    #error "Unsupported platform. DeviceFramework requires ESP8266 or ESP32."
#endif

// Platform-specific includes
#ifdef DF_PLATFORM_ESP8266
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
    #ifndef DF_DEFAULT_HOSTNAME
        #define DF_DEFAULT_HOSTNAME "esp8266"
    #endif
    #ifndef DF_DEFAULT_DEVICE_NAME
        #define DF_DEFAULT_DEVICE_NAME "esp8266-controller"
    #endif
    // ESP8266 uses system_rtc_mem_read/write
    #define DF_HAS_RTC_MEMORY
#elif defined(DF_PLATFORM_ESP32)
    #include <WiFi.h>
    #include <ESPmDNS.h>
    #ifndef DF_DEFAULT_HOSTNAME
        #define DF_DEFAULT_HOSTNAME "esp32"
    #endif
    #ifndef DF_DEFAULT_DEVICE_NAME
        #define DF_DEFAULT_DEVICE_NAME "esp32-controller"
    #endif
    // ESP32 will use Preferences API for persistent storage
    #define DF_HAS_PREFERENCES
    #include <Preferences.h>
#endif

// LED_BUILTIN detection and availability
#ifndef LED_BUILTIN
    // LED_BUILTIN not defined - LED functionality disabled
    #define DF_LED_AVAILABLE false
    #define DF_LED_PIN -1  // Invalid pin
#else
    #define DF_LED_AVAILABLE true
    #define DF_LED_PIN LED_BUILTIN

    // LED polarity handling (board-specific)
    // ESP8266: Typically active LOW
    // ESP32: Varies by board (nodemcu-32s: GPIO 2, active LOW)
    // Default behavior: assume active LOW for compatibility
    // User can override with build flags if needed: -DDF_LED_ACTIVE_LOW=false
    #ifndef DF_LED_ACTIVE_LOW
        #ifdef DF_PLATFORM_ESP8266
            #define DF_LED_ACTIVE_LOW true
        #elif defined(DF_PLATFORM_ESP32)
            // ESP32 boards vary - default to active LOW (works for nodemcu-32s)
            #define DF_LED_ACTIVE_LOW true
        #endif
    #endif
#endif

// Platform-specific API wrappers

// Chip ID - different APIs for each platform
#ifdef DF_PLATFORM_ESP8266
    #define DF_GET_CHIP_ID() ESP.getChipId()
#elif defined(DF_PLATFORM_ESP32)
    #define DF_GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#endif

// Hardware MAC - ESP32's station MAC is unavailable before Wi-Fi starts, so
// use its eFuse value during framework setup. ESP8266 exposes its MAC early.
inline void DFGetHardwareMac(uint8_t mac[6]) {
#ifdef DF_PLATFORM_ESP8266
    WiFi.macAddress(mac);
#elif defined(DF_PLATFORM_ESP32)
    const uint64_t efuseMac = ESP.getEfuseMac();
    for (uint8_t index = 0; index < 6; ++index) {
        mac[index] = static_cast<uint8_t>(efuseMac >> (index * 8));
    }
#endif
}

// HTTP Client includes (for test files)
#ifdef DF_PLATFORM_ESP8266
    #include <ESP8266HTTPClient.h>
#elif defined(DF_PLATFORM_ESP32)
    #include <HTTPClient.h>
#endif

// Memory statistics helper macros
// ESP8266: getHeapStats(&free, &max, &frag)
// ESP32: Uses different API - getFreeHeap() and getMaxAllocHeap()
#ifdef DF_PLATFORM_ESP8266
    #define DF_GET_HEAP_STATS(free, max, frag) ESP.getHeapStats(&(free), &(max), &(frag))
#elif defined(DF_PLATFORM_ESP32)
    // ESP32 doesn't have getHeapStats - use individual methods
    // ESP32 doesn't provide fragmentation - set to 0
    #define DF_GET_HEAP_STATS(free, max, frag) \
        do { \
            (free) = ESP.getFreeHeap(); \
            (max) = ESP.getMaxAllocHeap(); \
            (frag) = 0; \
        } while(0)
#endif

#endif // DEVICEFRAMEWORK_PLATFORM_H
