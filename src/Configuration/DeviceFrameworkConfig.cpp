#include "DeviceFrameworkConfig.h"

// General library configuration
char CONFIG_devicePassword[32] = "";
uint32_t CONFIG_configModeTimeout = 120000;
uint32_t CONFIG_configLEDToggleRate = 500;
uint32_t CONFIG_wifiReconnectInterval = 5000;   // Attempt WiFi reconnect every 5 seconds when offline
uint32_t CONFIG_rtcMagicNumber = 0xDEADBEEF;
uint16_t CONFIG_rtcMemAddr = 65;
uint32_t CONFIG_resetTimeout = 5000;
uint16_t CONFIG_eepromStart = 64;
uint16_t CONFIG_eepromSize = 1024;
uint32_t CONFIG_dnsResolutionTimeout = 2000;  // 2 seconds DNS timeout
uint32_t CONFIG_dnsCacheDuration = 300000;    // 5 minutes DNS cache
uint32_t CONFIG_mqttReconnectRateLimit = 5000; // 5 seconds between MQTT reconnection attempts
uint32_t CONFIG_mqttHAResyncInterval = 100;   // 100ms between paced HA parameter resyncs
uint32_t CONFIG_serialBaudRate = 9600; // Default serial baud rate

// Reset tracking configuration defaults
uint32_t CONFIG_resetCountTimeout = 300000; // 5 minutes - reset count expires after this
uint32_t CONFIG_apiStatusCacheInterval = 2000; // 2 seconds - API status cache interval
uint32_t CONFIG_heavyUpdateInterval = 30000; // 30 seconds - heavy status update interval

// WebSocket configuration defaults
uint32_t CONFIG_wsBufferSize = 1024;        // 1KB circular buffer
uint32_t CONFIG_wsSendInterval = 50;        // 50ms max wait between flushes
uint32_t CONFIG_wsClientCheckInterval = 50;  // 50ms client state check interval
uint32_t CONFIG_wsBackoffDelay = 200;       // 200ms backoff when all clients reject
uint32_t CONFIG_wsCleanupInterval = 30000;  // 30 seconds client cleanup interval

// mDNS configuration defaults
uint32_t CONFIG_mdnsMinFreeHeap = 4096;              // 4KB minimum free heap for ESP8266 mDNS work
uint32_t CONFIG_mdnsMinLargestBlock = 4096;          // 4KB contiguous allocation headroom for ESP8266 mDNS work
uint32_t CONFIG_mdnsUpdateInterval = 25;             // Bound expensive ESP8266 heap-stat scans to 40Hz
uint32_t CONFIG_mdnsPacketDrainMinFreeHeap = 4096;   // 4KB minimum free heap for resolver.loop()
uint32_t CONFIG_mdnsPacketDrainInterval = 2000;      // 2 seconds between packet drain operations

// Template engine configuration defaults
uint16_t CONFIG_templatePlaceholderNameSize = CONFIG_templatePlaceholderNameSize_default;
uint16_t CONFIG_maxTemplatePlaceholders = CONFIG_maxTemplatePlaceholders_default;
uint16_t CONFIG_templateStackDepth = CONFIG_templateStackDepth_default;
uint16_t CONFIG_templateBufferSize = CONFIG_templateBufferSize_default;
uint16_t CONFIG_templateMaxIterations = CONFIG_templateMaxIterations_default;
uint16_t CONFIG_templateProgmemChunkSize = CONFIG_templateProgmemChunkSize_default;
uint16_t CONFIG_templateRamChunkSize = CONFIG_templateRamChunkSize_default;


// Configuration getters and setters
const char* getConfigDevicePassword() {
    return CONFIG_devicePassword;
}

bool isConfigDevicePasswordValid(const char* password) {
    const size_t length = password ? strlen(password) : 0;
    return length == 0 || (length >= 8 && length < sizeof(CONFIG_devicePassword));
}

bool setConfigDevicePassword(const char* password) {
    if (!isConfigDevicePasswordValid(password)) return false;
    const char* value = password ? password : "";
    memcpy(CONFIG_devicePassword, value, strlen(value) + 1);
    return true;
}

uint32_t getConfigModeTimeout() {
    return CONFIG_configModeTimeout;
}

void setConfigModeTimeout(uint32_t timeout) {
    CONFIG_configModeTimeout = timeout;
}

uint32_t getConfigLEDToggleRate() {
    return CONFIG_configLEDToggleRate;
}

void setConfigLEDToggleRate(uint32_t rate) {
    CONFIG_configLEDToggleRate = rate;
}

uint32_t getConfigRTCMagicNumber() {
    return CONFIG_rtcMagicNumber;
}

void setConfigRTCMagicNumber(uint32_t magic) {
    CONFIG_rtcMagicNumber = magic;
}

uint16_t getConfigRTCMemAddr() {
    return CONFIG_rtcMemAddr;
}

void setConfigRTCMemAddr(uint16_t addr) {
    CONFIG_rtcMemAddr = addr;
}

uint32_t getConfigResetTimeout() {
    return CONFIG_resetTimeout;
}

void setConfigResetTimeout(uint32_t timeout) {
    CONFIG_resetTimeout = timeout;
}

uint16_t getConfigEEPROMStart() {
    return CONFIG_eepromStart;
}

void setConfigEEPROMStart(uint16_t start) {
    CONFIG_eepromStart = start;
}

uint16_t getConfigEEPROMSize() {
    return CONFIG_eepromSize;
}

void setConfigEEPROMSize(uint16_t size) {
    CONFIG_eepromSize = size;
}

uint32_t getConfigDNSResolutionTimeout() {
    return CONFIG_dnsResolutionTimeout;
}

void setConfigDNSResolutionTimeout(uint32_t timeout) {
    CONFIG_dnsResolutionTimeout = timeout;
}

uint32_t getConfigDNSCacheDuration() {
    return CONFIG_dnsCacheDuration;
}

void setConfigDNSCacheDuration(uint32_t duration) {
    CONFIG_dnsCacheDuration = duration;
}

uint32_t getConfigMQTTReconnectRateLimit() {
    return CONFIG_mqttReconnectRateLimit;
}

void setConfigMQTTReconnectRateLimit(uint32_t rateLimit) {
    CONFIG_mqttReconnectRateLimit = rateLimit;
}

uint32_t getConfigMQTTHAResyncInterval() {
    return CONFIG_mqttHAResyncInterval;
}

void setConfigMQTTHAResyncInterval(uint32_t interval) {
    if (interval <= 5000) {
        CONFIG_mqttHAResyncInterval = interval;
    }
}

uint32_t getConfigWiFiReconnectInterval() {
    return CONFIG_wifiReconnectInterval;
}

void setConfigWiFiReconnectInterval(uint32_t interval) {
    CONFIG_wifiReconnectInterval = interval;
}

uint32_t getConfigSerialBaudRate() {
    return CONFIG_serialBaudRate;
}

void setConfigSerialBaudRate(uint32_t baudRate) {
    CONFIG_serialBaudRate = baudRate;
}

// Reset tracking configuration getters and setters
uint32_t getConfigResetCountTimeout() {
    return CONFIG_resetCountTimeout;
}

void setConfigResetCountTimeout(uint32_t timeout) {
    CONFIG_resetCountTimeout = timeout;
}

uint32_t getConfigAPIStatusCacheInterval() {
    return CONFIG_apiStatusCacheInterval;
}

void setConfigAPIStatusCacheInterval(uint32_t interval) {
    CONFIG_apiStatusCacheInterval = interval;
}

uint32_t getConfigHeavyUpdateInterval() {
    return CONFIG_heavyUpdateInterval;
}

void setConfigHeavyUpdateInterval(uint32_t interval) {
    CONFIG_heavyUpdateInterval = interval;
}

// WebSocket configuration getters and setters
uint32_t getConfigWSBufferSize() {
    return CONFIG_wsBufferSize;
}

void setConfigWSBufferSize(uint32_t size) {
    if (size >= 256 && size <= 4096) { // Reasonable limits
        CONFIG_wsBufferSize = size;
    }
}

uint32_t getConfigWSSendInterval() {
    return CONFIG_wsSendInterval;
}

void setConfigWSSendInterval(uint32_t interval) {
    if (interval >= 10 && interval <= 1000) { // 10ms to 1s
        CONFIG_wsSendInterval = interval;
    }
}

uint32_t getConfigWSClientCheckInterval() {
    return CONFIG_wsClientCheckInterval;
}

void setConfigWSClientCheckInterval(uint32_t interval) {
    if (interval >= 10 && interval <= 1000) { // 10ms to 1s
        CONFIG_wsClientCheckInterval = interval;
    }
}

uint32_t getConfigWSBackoffDelay() {
    return CONFIG_wsBackoffDelay;
}

void setConfigWSBackoffDelay(uint32_t delay) {
    if (delay >= 50 && delay <= 2000) { // 50ms to 2s
        CONFIG_wsBackoffDelay = delay;
    }
}

uint32_t getConfigWSCleanupInterval() {
    return CONFIG_wsCleanupInterval;
}

void setConfigWSCleanupInterval(uint32_t interval) {
    if (interval >= 5000 && interval <= 300000) { // 5s to 5min
        CONFIG_wsCleanupInterval = interval;
    }
}

// mDNS configuration getters and setters
uint32_t getConfigMDNSMinFreeHeap() {
    return CONFIG_mdnsMinFreeHeap;
}

void setConfigMDNSMinFreeHeap(uint32_t minFreeHeap) {
    CONFIG_mdnsMinFreeHeap = minFreeHeap;
}

uint32_t getConfigMDNSMinLargestBlock() {
    return CONFIG_mdnsMinLargestBlock;
}

void setConfigMDNSMinLargestBlock(uint32_t minLargestBlock) {
    CONFIG_mdnsMinLargestBlock = minLargestBlock;
}

uint32_t getConfigMDNSUpdateInterval() {
    return CONFIG_mdnsUpdateInterval;
}

void setConfigMDNSUpdateInterval(uint32_t interval) {
    if (interval >= 10 && interval <= 1000) { // 10ms to 1s
        CONFIG_mdnsUpdateInterval = interval;
    }
}

uint32_t getConfigMDNSPacketDrainMinFreeHeap() {
    return CONFIG_mdnsPacketDrainMinFreeHeap;
}

void setConfigMDNSPacketDrainMinFreeHeap(uint32_t minFreeHeap) {
    CONFIG_mdnsPacketDrainMinFreeHeap = minFreeHeap;
}

uint32_t getConfigMDNSPacketDrainInterval() {
    return CONFIG_mdnsPacketDrainInterval;
}

void setConfigMDNSPacketDrainInterval(uint32_t interval) {
    if (interval >= 100 && interval <= 60000) { // 100ms to 60s
        CONFIG_mdnsPacketDrainInterval = interval;
    }
}

// Template engine configuration getters and setters
uint16_t getConfigTemplatePlaceholderNameSize() {
    return CONFIG_templatePlaceholderNameSize;
}

void setConfigTemplatePlaceholderNameSize(uint16_t size) {
    if (size >= 3 && size <= 128) { // Allow minimal placeholders like "%X%"
        CONFIG_templatePlaceholderNameSize = size;
    }
}

uint16_t getConfigMaxTemplatePlaceholders() {
    return CONFIG_maxTemplatePlaceholders;
}

void setConfigMaxTemplatePlaceholders(uint16_t maxPlaceholders) {
    if (maxPlaceholders >= 1 && maxPlaceholders <= 64) { // Allow at least 1
        CONFIG_maxTemplatePlaceholders = maxPlaceholders;
    }
}

uint16_t getConfigTemplateStackDepth() {
    return CONFIG_templateStackDepth;
}

void setConfigTemplateStackDepth(uint16_t depth) {
    if (depth >= 1 && depth <= 64) { // Allow at least 1, reasonable max
        CONFIG_templateStackDepth = depth;
    }
}

uint16_t getConfigTemplateBufferSize() {
    return CONFIG_templateBufferSize;
}

void setConfigTemplateBufferSize(uint16_t size) {
    if (size >= 128 && size <= 4096) { // Reasonable limits (128 bytes to 4KB)
        CONFIG_templateBufferSize = size;
    }
}

uint16_t getConfigTemplateMaxIterations() {
    return CONFIG_templateMaxIterations;
}

void setConfigTemplateMaxIterations(uint16_t iterations) {
    if (iterations >= 10 && iterations <= 200) { // Reasonable limits (10 to 200)
        CONFIG_templateMaxIterations = iterations;
    }
}

uint16_t getConfigTemplateProgmemChunkSize() {
    return CONFIG_templateProgmemChunkSize;
}

void setConfigTemplateProgmemChunkSize(uint16_t size) {
    if (size >= 64 && size <= 2048) { // Reasonable limits (64 bytes to 2KB)
        CONFIG_templateProgmemChunkSize = size;
    }
}

uint16_t getConfigTemplateRamChunkSize() {
    return CONFIG_templateRamChunkSize;
}

void setConfigTemplateRamChunkSize(uint16_t size) {
    if (size >= 32 && size <= 1024) { // Reasonable limits (32 bytes to 1KB)
        CONFIG_templateRamChunkSize = size;
    }
}
