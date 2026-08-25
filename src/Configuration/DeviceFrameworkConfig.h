#ifndef DEVICEFRAMEWORK_CONFIG_H
#define DEVICEFRAMEWORK_CONFIG_H

#include <Arduino.h>

// General library configuration
// One optional password shared by the WiFi configuration portal, Arduino OTA,
// HTTP Basic authentication, and WebSerial. Leave it empty only when the
// device is intentionally managed on an open local network. WiFi access-point
// security requires 8–31 characters when a password is enabled.
extern char CONFIG_adminPassword[32];
extern uint32_t CONFIG_configModeTimeout;
extern uint32_t CONFIG_configLEDToggleRate;
extern uint32_t CONFIG_wifiReconnectInterval;
extern uint32_t CONFIG_rtcMagicNumber;
extern uint16_t CONFIG_rtcMemAddr;
extern uint32_t CONFIG_resetTimeout;
extern uint16_t CONFIG_eepromStart;
extern uint16_t CONFIG_eepromSize;
extern uint32_t CONFIG_dnsResolutionTimeout;
extern uint32_t CONFIG_dnsCacheDuration;
extern uint32_t CONFIG_mqttReconnectRateLimit;
extern uint32_t CONFIG_mqttHAResyncInterval;
extern uint32_t CONFIG_serialBaudRate;

// Reset tracking configuration
extern uint32_t CONFIG_resetCountTimeout;
extern uint32_t CONFIG_apiStatusCacheInterval;
extern uint32_t CONFIG_heavyUpdateInterval;

// WebSocket configuration
extern uint32_t CONFIG_wsBufferSize;
extern uint32_t CONFIG_wsSendInterval;
extern uint32_t CONFIG_wsClientCheckInterval;
extern uint32_t CONFIG_wsBackoffDelay;
extern uint32_t CONFIG_wsCleanupInterval;

// mDNS configuration
extern uint32_t CONFIG_mdnsMinFreeHeap;
extern uint32_t CONFIG_mdnsPacketDrainMinFreeHeap;
extern uint32_t CONFIG_mdnsPacketDrainInterval;

// Template engine configuration
extern uint16_t CONFIG_templatePlaceholderNameSize;
extern uint16_t CONFIG_maxTemplatePlaceholders;
extern uint16_t CONFIG_templateStackDepth;
extern uint16_t CONFIG_templateBufferSize;
extern uint16_t CONFIG_templateMaxIterations;
extern uint16_t CONFIG_templateProgmemChunkSize;
extern uint16_t CONFIG_templateRamChunkSize;
// Template engine configuration defaults
#define CONFIG_templatePlaceholderNameSize_default 24   // Allows %X% style (min 3)
#define CONFIG_maxTemplatePlaceholders_default 16       // Current usage is 13
#define CONFIG_templateStackDepth_default 16            // Max nested template depth
#define CONFIG_templateBufferSize_default 512          // Template read buffer size
#define CONFIG_templateMaxIterations_default 50         // Max iterations per render chunk (safety limit)
#define CONFIG_templateProgmemChunkSize_default 512     // Max chunk size for PROGMEM data copying
#define CONFIG_templateRamChunkSize_default 128         // Max chunk size for RAM data copying

// RTC memory structure
typedef struct {
    uint32_t crc32;        // CRC32 checksum
    uint32_t magic;        // A unique magic number to identify our data
    uint32_t lastReset;    // Timestamp of the last reset
    uint8_t resetCount;    // Number of resets within the time window
    uint32_t totalResetCount; // Total resets since last timeout (persistent across resets)
} RtcData;

// Configuration getters and setters
// The Admin names are retained as compatibility aliases for the shared device password.
const char* getConfigAdminPassword();
bool setConfigAdminPassword(const char* password);
const char* getConfigDevicePassword();
// Empty disables local authentication. A non-empty value must contain 8–31 characters
// so the WiFi provisioning AP, OTA, HTTP Basic authentication, and WebSerial agree.
bool setConfigDevicePassword(const char* password);
uint32_t getConfigModeTimeout();
void setConfigModeTimeout(uint32_t timeout);
uint32_t getConfigLEDToggleRate();
void setConfigLEDToggleRate(uint32_t rate);
uint32_t getConfigRTCMagicNumber();
void setConfigRTCMagicNumber(uint32_t magic);
uint16_t getConfigRTCMemAddr();
void setConfigRTCMemAddr(uint16_t addr);
uint32_t getConfigResetTimeout();
void setConfigResetTimeout(uint32_t timeout);
uint16_t getConfigEEPROMStart();
void setConfigEEPROMStart(uint16_t start);
uint16_t getConfigEEPROMSize();
void setConfigEEPROMSize(uint16_t size);
uint32_t getConfigDNSResolutionTimeout();
void setConfigDNSResolutionTimeout(uint32_t timeout);
uint32_t getConfigDNSCacheDuration();
void setConfigDNSCacheDuration(uint32_t duration);
uint32_t getConfigMQTTReconnectRateLimit();
void setConfigMQTTReconnectRateLimit(uint32_t rateLimit);
uint32_t getConfigMQTTHAResyncInterval();
void setConfigMQTTHAResyncInterval(uint32_t interval);
uint32_t getConfigWiFiReconnectInterval();
void setConfigWiFiReconnectInterval(uint32_t interval);
uint32_t getConfigSerialBaudRate();
void setConfigSerialBaudRate(uint32_t baudRate);

// Reset tracking configuration getters and setters
uint32_t getConfigResetCountTimeout();
void setConfigResetCountTimeout(uint32_t timeout);
uint32_t getConfigAPIStatusCacheInterval();
void setConfigAPIStatusCacheInterval(uint32_t interval);
uint32_t getConfigHeavyUpdateInterval();
void setConfigHeavyUpdateInterval(uint32_t interval);

// WebSocket configuration getters and setters
uint32_t getConfigWSBufferSize();
void setConfigWSBufferSize(uint32_t size);
uint32_t getConfigWSSendInterval();
void setConfigWSSendInterval(uint32_t interval);
uint32_t getConfigWSClientCheckInterval();
void setConfigWSClientCheckInterval(uint32_t interval);
uint32_t getConfigWSBackoffDelay();
void setConfigWSBackoffDelay(uint32_t delay);
uint32_t getConfigWSCleanupInterval();
void setConfigWSCleanupInterval(uint32_t interval);

// mDNS configuration getters and setters
uint32_t getConfigMDNSMinFreeHeap();
void setConfigMDNSMinFreeHeap(uint32_t minFreeHeap);
uint32_t getConfigMDNSPacketDrainMinFreeHeap();
void setConfigMDNSPacketDrainMinFreeHeap(uint32_t minFreeHeap);
uint32_t getConfigMDNSPacketDrainInterval();
void setConfigMDNSPacketDrainInterval(uint32_t interval);

// Template engine configuration getters and setters
uint16_t getConfigTemplatePlaceholderNameSize();
void setConfigTemplatePlaceholderNameSize(uint16_t size);
uint16_t getConfigMaxTemplatePlaceholders();
void setConfigMaxTemplatePlaceholders(uint16_t maxPlaceholders);
uint16_t getConfigTemplateStackDepth();
void setConfigTemplateStackDepth(uint16_t depth);
uint16_t getConfigTemplateBufferSize();
void setConfigTemplateBufferSize(uint16_t size);
uint16_t getConfigTemplateMaxIterations();
void setConfigTemplateMaxIterations(uint16_t iterations);
uint16_t getConfigTemplateProgmemChunkSize();
void setConfigTemplateProgmemChunkSize(uint16_t size);
uint16_t getConfigTemplateRamChunkSize();
void setConfigTemplateRamChunkSize(uint16_t size);

#endif // DEVICEFRAMEWORK_CONFIG_H
