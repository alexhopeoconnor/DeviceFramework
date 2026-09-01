#ifdef ENABLE_WEB_INTERFACE
#include "DeviceFrameworkDeviceStatus.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../Configuration/DeviceFrameworkIdentity.h"
#include "../MQTT/DeviceFrameworkMQTT.h"
#include "../Utils/TimeUtils.h"
#include "../Utils/PrintAdapters.h"
#include "../Storage/DeviceFrameworkRTC.h"
#include <DeviceFrameworkPlatform.h>

namespace {

class JSONChunkPrint final : public Print {
public:
    JSONChunkPrint(uint8_t* buffer, size_t capacity, size_t skip)
        : buffer(buffer), capacity(capacity), skip(skip), total(0), written(0) {}

    size_t write(uint8_t byte) override {
        if (total >= skip && written < capacity) {
            buffer[written++] = byte;
        }
        ++total;
        return 1;
    }

    size_t write(const uint8_t* data, size_t length) override {
        if (total + length > skip && written < capacity) {
            const size_t sourceOffset = total < skip ? skip - total : 0;
            const size_t available = length - sourceOffset;
            const size_t toCopy = min(available, capacity - written);
            if (toCopy > 0) {
                memcpy(buffer + written, data + sourceOffset, toCopy);
                written += toCopy;
            }
        }
        total += length;
        return length;
    }

    size_t bytesWritten() const { return written; }
    size_t totalBytes() const { return total; }

private:
    uint8_t* buffer;
    size_t capacity;
    size_t skip;
    size_t total;
    size_t written;
};

template <size_t Size>
void copyText(char (&destination)[Size], const char* source) {
    if (!source) {
        destination[0] = '\0';
        return;
    }

    size_t index = 0;
    while (index + 1 < Size && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

template <size_t Size>
void formatIPAddress(char (&destination)[Size], const IPAddress& address) {
    snprintf(destination, Size, "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
}

template <size_t Size>
void formatMacAddress(char (&destination)[Size], const uint8_t* address) {
    if (!address) {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, Size, "%02X:%02X:%02X:%02X:%02X:%02X",
        address[0], address[1], address[2], address[3], address[4], address[5]);
}

void writeJSONString(Print& output, const char* value) {
    output.write(static_cast<uint8_t>('"'));
    if (!value) {
        value = "";
    }

    while (*value != '\0') {
        const unsigned char character = static_cast<unsigned char>(*value++);
        switch (character) {
            case '"': output.print(F("\\\"")); break;
            case '\\': output.print(F("\\\\")); break;
            case '\b': output.print(F("\\b")); break;
            case '\f': output.print(F("\\f")); break;
            case '\n': output.print(F("\\n")); break;
            case '\r': output.print(F("\\r")); break;
            case '\t': output.print(F("\\t")); break;
            default:
                if (character < 0x20) {
                    static const char hex[] = "0123456789ABCDEF";
                    output.print(F("\\u00"));
                    output.write(hex[(character >> 4) & 0x0F]);
                    output.write(hex[character & 0x0F]);
                } else {
                    output.write(character);
                }
                break;
        }
    }
    output.write(static_cast<uint8_t>('"'));
}

} // namespace

// Static member definitions
DeviceStatus DeviceStatusManager::deviceStatus;
bool DeviceStatusManager::hardwareInitialized = false;
MemoryStats DeviceStatusManager::lastMemoryStats;

// Connection tracking for runtime metrics (RAM only)
bool DeviceStatusManager::lastWiFiConnected = false;
bool DeviceStatusManager::lastMQTTConnected = false;
unsigned long DeviceStatusManager::wifiConnectionStartTime = 0;
unsigned long DeviceStatusManager::mqttConnectionStartTime = 0;
uint32_t DeviceStatusManager::wifiDisconnectionCount = 0;
uint32_t DeviceStatusManager::mqttDisconnectionCount = 0;
unsigned long DeviceStatusManager::lastWifiDisconnectionTime = 0;
unsigned long DeviceStatusManager::lastMqttDisconnectionTime = 0;

// Runtime performance tracking (RAM only)
unsigned long DeviceStatusManager::lastStatusUpdateTime = 0;
uint32_t DeviceStatusManager::statusUpdateCount = 0;
int32_t DeviceStatusManager::freeHeapTrendPerMinute = 0;
uint32_t DeviceStatusManager::highestFreeHeap = 0;
uint32_t DeviceStatusManager::lowestFreeHeap = 0;
unsigned long DeviceStatusManager::lastHeapSampleTime = 0;
DeviceStatusManager::HeapSample DeviceStatusManager::heapHistory[DeviceStatusManager::heapSampleCount] = {};
uint8_t DeviceStatusManager::heapHistoryLength = 0;
uint8_t DeviceStatusManager::heapHistoryNext = 0;
unsigned long DeviceStatusManager::bootTime = 0;

// Cached estimation data
size_t DeviceStatusManager::cachedJSONSize = 0;
bool DeviceStatusManager::sizeEstimationInitialized = false;

void DeviceStatusManager::initializeHardwareInfo() {
    if (hardwareInitialized) return;

    // Platform-specific hardware info
    // Get max memory - ESP32 can detect at runtime, ESP8266 uses hardcoded default
    #ifdef DF_PLATFORM_ESP8266
        // ESP8266: No runtime API for total heap size - use hardcoded default
        // Most ESP8266 boards have ~80KB total heap
        deviceStatus.hardware.maxMemory = 81920;
    #elif defined(DF_PLATFORM_ESP32)
        // ESP32: getHeapSize() returns total heap size at runtime - works for all ESP32 variants
        deviceStatus.hardware.maxMemory = ESP.getHeapSize();
    #endif
    deviceStatus.hardware.chipId = String(DF_GET_CHIP_ID());

    // Most hardware info is the same across platforms
    deviceStatus.hardware.flashSize = ESP.getFlashChipSize();
    deviceStatus.hardware.cpuFreq = ESP.getCpuFreqMHz();
    deviceStatus.hardware.sketchSize = ESP.getSketchSize();
    deviceStatus.hardware.freeSketchSpace = ESP.getFreeSketchSpace();

    // Flash speed differs: ESP8266 returns MHz, ESP32 returns Hz
    #ifdef DF_PLATFORM_ESP8266
        deviceStatus.hardware.flashSpeed = ESP.getFlashChipSpeed();
    #elif defined(DF_PLATFORM_ESP32)
        deviceStatus.hardware.flashSpeed = ESP.getFlashChipSpeed() / 1000000; // Convert Hz to MHz
    #endif

    const DeviceFrameworkApplicationIdentity& application = DeviceFrameworkIdentity::getApplication();
    deviceStatus.hardware.version = String(application.firmwareVersion) + F(" (DF ") + DeviceFrameworkIdentity::getLibraryVersion() + F(")");

    hardwareInitialized = true;
}

void DeviceStatusManager::initializeRuntimeTracking() {
    bootTime = millis();
    lastStatusUpdateTime = 0;
    statusUpdateCount = 0;
    freeHeapTrendPerMinute = 0;
    highestFreeHeap = 0;
    lowestFreeHeap = 0;
    lastHeapSampleTime = 0;
    heapHistoryLength = 0;
    heapHistoryNext = 0;
    wifiDisconnectionCount = 0;
    mqttDisconnectionCount = 0;

    // Initialize connection tracking
    lastWiFiConnected = WiFi.isConnected();
    lastMQTTConnected = DeviceFrameworkMQTT::isConnected();

    if (lastWiFiConnected) {
        wifiConnectionStartTime = millis();
    }
    if (lastMQTTConnected) {
        mqttConnectionStartTime = millis();
    }
}

void DeviceStatusManager::trackConnectionChanges() {
    bool currentWiFiConnected = WiFi.isConnected();
    bool currentMQTTConnected = DeviceFrameworkMQTT::isConnected();

    // Track WiFi disconnections
    if (lastWiFiConnected && !currentWiFiConnected) {
        wifiDisconnectionCount++;
        lastWifiDisconnectionTime = millis();
    }

    // Track MQTT disconnections
    if (lastMQTTConnected && !currentMQTTConnected) {
        mqttDisconnectionCount++;
        lastMqttDisconnectionTime = millis();
    }

    // Update connection start times
    if (!lastWiFiConnected && currentWiFiConnected) {
        wifiConnectionStartTime = millis();
    }
    if (!lastMQTTConnected && currentMQTTConnected) {
        mqttConnectionStartTime = millis();
    }

    lastWiFiConnected = currentWiFiConnected;
    lastMQTTConnected = currentMQTTConnected;
}

void DeviceStatusManager::updateRuntimeMetrics() {
    unsigned long currentTime = millis();

    statusUpdateCount++;
    lastStatusUpdateTime = currentTime;
    uint32_t currentHeap = ESP.getFreeHeap();

    if (highestFreeHeap == 0 || currentHeap > highestFreeHeap) {
        highestFreeHeap = currentHeap;
    }
    if (lowestFreeHeap == 0 || currentHeap < lowestFreeHeap) {
        lowestFreeHeap = currentHeap;
    }

    if (lastHeapSampleTime != 0 &&
        !TimeUtils::hasTimeElapsed(currentTime, lastHeapSampleTime, heapSampleInterval)) {
        return;
    }

    lastHeapSampleTime = currentTime;
    heapHistory[heapHistoryNext].freeHeap = currentHeap;
    heapHistory[heapHistoryNext].timestamp = currentTime;
    if (heapHistoryLength < heapSampleCount) {
        ++heapHistoryLength;
    }
    heapHistoryNext = (heapHistoryNext + 1) % heapSampleCount;

    // Startup allocates framework services lazily. Do not diagnose that normal
    // settling period as a leak until the full, fixed sampling window exists.
    if (heapHistoryLength < heapSampleCount) {
        freeHeapTrendPerMinute = 0;
        return;
    }

    const uint8_t oldestIndex = heapHistoryLength == heapSampleCount ? heapHistoryNext : 0;
    const HeapSample& oldestSample = heapHistory[oldestIndex];
    const unsigned long elapsed = TimeUtils::safeTimeDifference(currentTime, oldestSample.timestamp);
    if (elapsed == 0) {
        freeHeapTrendPerMinute = 0;
        return;
    }

    const int64_t difference = static_cast<int64_t>(currentHeap) - oldestSample.freeHeap;
    const int64_t trend = (difference * 60000LL) / elapsed;
    if (trend > 2147483647LL) {
        freeHeapTrendPerMinute = 2147483647;
    } else if (trend < -2147483648LL) {
        freeHeapTrendPerMinute = -2147483647 - 1;
    } else {
        freeHeapTrendPerMinute = static_cast<int32_t>(trend);
    }
}

void DeviceStatusManager::updateRuntimeInfo() {
    unsigned long now = millis();

    // Track connection changes
    trackConnectionChanges();

    // Update runtime metrics (lightweight)
    updateRuntimeMetrics();

    // Only update heavy WiFi/MQTT info if we're actually serving requests
    // or if significant time has passed (configurable interval)
    static unsigned long lastHeavyUpdate = 0;
    bool needsHeavyUpdate = (now - lastHeavyUpdate > getConfigHeavyUpdateInterval()) || needsUpdate();

    if (needsHeavyUpdate) {
        // Read RTC data for reset information only
        RtcData rtcData;
        DeviceFrameworkRTC::read(&rtcData);

        // WiFi stability
        deviceStatus.runtime.wifi.connected = WiFi.isConnected();
        const String ssid = WiFi.SSID();
        copyText(deviceStatus.runtime.wifi.ssid, ssid.c_str());
        formatIPAddress(deviceStatus.runtime.wifi.ip, WiFi.localIP());
        formatIPAddress(deviceStatus.runtime.wifi.gateway, WiFi.gatewayIP());
        formatIPAddress(deviceStatus.runtime.wifi.subnet, WiFi.subnetMask());
        formatIPAddress(deviceStatus.runtime.wifi.dns, WiFi.dnsIP());
        uint8_t mac[6];
        WiFi.macAddress(mac);
        formatMacAddress(deviceStatus.runtime.wifi.mac, mac);
        deviceStatus.runtime.wifi.rssi = WiFi.RSSI();
        deviceStatus.runtime.wifi.channel = WiFi.channel();
        formatMacAddress(deviceStatus.runtime.wifi.bssid, WiFi.BSSID());

    // Calculate connection duration using TimeUtils
    if (deviceStatus.runtime.wifi.connected && wifiConnectionStartTime > 0) {
        deviceStatus.runtime.wifi.connectionDuration = TimeUtils::safeTimeDifference(now, wifiConnectionStartTime) / 1000;
    } else {
        deviceStatus.runtime.wifi.connectionDuration = 0;
    }

        deviceStatus.runtime.wifi.totalDisconnections = wifiDisconnectionCount;
        deviceStatus.runtime.wifi.lastDisconnectionTime = lastWifiDisconnectionTime;
        deviceStatus.runtime.wifi.isStable = (wifiDisconnectionCount < 5 &&
                                             deviceStatus.runtime.wifi.connectionDuration > 300); // 5 minutes

        // MQTT stability
        deviceStatus.runtime.mqtt.connected = DeviceFrameworkMQTT::isConnected();
        copyText(deviceStatus.runtime.mqtt.broker, DeviceFrameworkParameters::getMqttServer());
        deviceStatus.runtime.mqtt.port = DeviceFrameworkParameters::getMqttPort();
        copyText(deviceStatus.runtime.mqtt.user, DeviceFrameworkParameters::getMqttUser());

        // Calculate MQTT connection duration using TimeUtils
        if (deviceStatus.runtime.mqtt.connected && mqttConnectionStartTime > 0) {
            deviceStatus.runtime.mqtt.connectionDuration = TimeUtils::safeTimeDifference(now, mqttConnectionStartTime) / 1000;
        } else {
            deviceStatus.runtime.mqtt.connectionDuration = 0;
        }

        deviceStatus.runtime.mqtt.totalDisconnections = mqttDisconnectionCount;
        deviceStatus.runtime.mqtt.lastDisconnectionTime = lastMqttDisconnectionTime;
        deviceStatus.runtime.mqtt.isStable = (mqttDisconnectionCount < 3 &&
                                              deviceStatus.runtime.mqtt.connectionDuration > 60); // 1 minute

        // Stability info from RTC (minimal)
        deviceStatus.runtime.stability.resetCount = rtcData.resetCount;
        deviceStatus.runtime.stability.timeSinceLastReset = TimeUtils::safeTimeDifference(now, rtcData.lastReset);
        deviceStatus.runtime.stability.totalUptime = TimeUtils::safeTimeDifference(now, bootTime) / 1000;
        deviceStatus.runtime.stability.isStable = (rtcData.resetCount <= 2 &&
                                                   deviceStatus.runtime.stability.timeSinceLastReset > getConfigResetCountTimeout());

        lastHeavyUpdate = now;
    }

    // Always update lightweight device info
    copyText(deviceStatus.runtime.device.deviceName, DeviceFrameworkParameters::getDeviceName());
    deviceStatus.runtime.device.uptime = TimeUtils::safeTimeDifference(now, bootTime) / 1000;

    // Enhanced memory info
    MemoryStats currentStats = getMemoryStats();
    deviceStatus.runtime.memory.freeMemory = currentStats.freeHeap;
    deviceStatus.runtime.memory.maxBlockSize = currentStats.maxBlock;
    deviceStatus.runtime.memory.fragmentation = currentStats.fragmentation;

    // Calculate memory delta using TimeUtils
    if (deviceStatus.lastMemoryCheck > 0) {
        deviceStatus.runtime.memory.freeHeapDelta = static_cast<int32_t>(currentStats.freeHeap) - static_cast<int32_t>(lastMemoryStats.freeHeap);
    }
    lastMemoryStats = currentStats;
    deviceStatus.lastMemoryCheck = now;

    // Runtime memory tracking (RAM only)
    deviceStatus.runtime.memory.highestFreeHeap = highestFreeHeap;
    deviceStatus.runtime.memory.lowestFreeHeap = lowestFreeHeap;

    // Logging info
    deviceStatus.runtime.logging.serialEnabled = isSerialActive();
    copyText(deviceStatus.runtime.logging.currentLogLevel, logLevelToString(currentLogLevel));
    deviceStatus.runtime.logging.webSerialEnabled = true; // Will be updated by web interface

    // System health
    deviceStatus.runtime.health.statusUpdateCount = statusUpdateCount;
    deviceStatus.runtime.health.lastStatusUpdateTime = lastStatusUpdateTime;
    deviceStatus.runtime.health.freeHeapTrendPerMinute = freeHeapTrendPerMinute;
    deviceStatus.runtime.health.systemHealthy = (freeHeapTrendPerMinute > -1024 && // Not losing more than 1 KiB/minute
                                                 deviceStatus.runtime.memory.fragmentation < 50); // Less than 50% fragmentation

    deviceStatus.lastUpdate = now;
}

bool DeviceStatusManager::needsUpdate() {
    return (deviceStatus.lastUpdate == 0 ||
            TimeUtils::hasTimeElapsed(millis(), deviceStatus.lastUpdate, getConfigAPIStatusCacheInterval()));
}

const DeviceStatus& DeviceStatusManager::getStatus() {
    return deviceStatus;
}

// Stream JSON directly to Print interface - memory efficient, no String allocations
void DeviceStatusManager::buildJSONResponse(Print& output, const DeviceStatus& status) {
    // Use Print::print() with PROGMEM strings directly - no String allocations
    output.print(F("{"));

    // Hardware info (static)
    output.print(F("\"hardware\":{"));
    output.print(F("\"max_memory\":"));
    output.print(status.hardware.maxMemory);
    output.print(F(",\"chip_id\":"));
    writeJSONString(output, status.hardware.chipId.c_str());
    output.print(F(",\"flash_size\":"));
    output.print(status.hardware.flashSize);
    output.print(F(",\"flash_speed\":"));
    output.print(status.hardware.flashSpeed);
    output.print(F(",\"cpu_freq\":"));
    output.print(status.hardware.cpuFreq);
    output.print(F(",\"sketch_size\":"));
    output.print(status.hardware.sketchSize);
    output.print(F(",\"free_sketch_space\":"));
    output.print(status.hardware.freeSketchSpace);
    output.print(F(",\"version\":"));
    writeJSONString(output, status.hardware.version.c_str());
    output.print(F("},"));

    // Runtime info (dynamic)
    output.print(F("\"runtime\":{"));

    // WiFi stability section
    output.print(F("\"wifi\":{"));
    output.print(F("\"connected\":"));
    output.print(status.runtime.wifi.connected ? F("true") : F("false"));
    output.print(F(",\"ssid\":"));
    writeJSONString(output, status.runtime.wifi.ssid);
    output.print(F(",\"ip\":"));
    writeJSONString(output, status.runtime.wifi.ip);
    output.print(F(",\"gateway\":"));
    writeJSONString(output, status.runtime.wifi.gateway);
    output.print(F(",\"subnet\":"));
    writeJSONString(output, status.runtime.wifi.subnet);
    output.print(F(",\"dns\":"));
    writeJSONString(output, status.runtime.wifi.dns);
    output.print(F(",\"mac\":"));
    writeJSONString(output, status.runtime.wifi.mac);
    output.print(F(",\"rssi\":"));
    output.print(status.runtime.wifi.rssi);
    output.print(F(",\"channel\":"));
    output.print(status.runtime.wifi.channel);
    output.print(F(",\"bssid\":"));
    writeJSONString(output, status.runtime.wifi.bssid);
    output.print(F(",\"connection_duration\":"));
    output.print(status.runtime.wifi.connectionDuration);
    output.print(F(",\"total_disconnections\":"));
    output.print(status.runtime.wifi.totalDisconnections);
    output.print(F(",\"last_disconnection_time\":"));
    output.print(status.runtime.wifi.lastDisconnectionTime);
    output.print(F(",\"is_stable\":"));
    output.print(status.runtime.wifi.isStable ? F("true") : F("false"));
    output.print(F("},"));

    // MQTT stability section
    output.print(F("\"mqtt\":{"));
    output.print(F("\"connected\":"));
    output.print(status.runtime.mqtt.connected ? F("true") : F("false"));
    output.print(F(",\"broker\":"));
    writeJSONString(output, status.runtime.mqtt.broker);
    output.print(F(",\"port\":"));
    output.print(status.runtime.mqtt.port);
    output.print(F(",\"user\":"));
    writeJSONString(output, status.runtime.mqtt.user);
    output.print(F(",\"connection_duration\":"));
    output.print(status.runtime.mqtt.connectionDuration);
    output.print(F(",\"total_disconnections\":"));
    output.print(status.runtime.mqtt.totalDisconnections);
    output.print(F(",\"last_disconnection_time\":"));
    output.print(status.runtime.mqtt.lastDisconnectionTime);
    output.print(F(",\"is_stable\":"));
    output.print(status.runtime.mqtt.isStable ? F("true") : F("false"));
    output.print(F("},"));

    // Device section
    output.print(F("\"device\":{"));
    output.print(F("\"name\":"));
    writeJSONString(output, status.runtime.device.deviceName);
    output.print(F(",\"uptime\":"));
    output.print(status.runtime.device.uptime);
    output.print(F("},"));

    // Enhanced memory section
    output.print(F("\"memory\":{"));
    output.print(F("\"free\":"));
    output.print(status.runtime.memory.freeMemory);
    output.print(F(",\"max_block\":"));
    output.print(status.runtime.memory.maxBlockSize);
    output.print(F(",\"fragmentation\":"));
    output.print(status.runtime.memory.fragmentation);
    output.print(F(",\"free_heap_delta\":"));
    output.print(status.runtime.memory.freeHeapDelta);
    output.print(F(",\"highest_free\":"));
    output.print(status.runtime.memory.highestFreeHeap);
    output.print(F(",\"lowest_free\":"));
    output.print(status.runtime.memory.lowestFreeHeap);
    output.print(F("},"));

    // Stability section
    output.print(F("\"stability\":{"));
    output.print(F("\"reset_count\":"));
    output.print(status.runtime.stability.resetCount);
    output.print(F(",\"time_since_reset\":"));
    output.print(status.runtime.stability.timeSinceLastReset);
    output.print(F(",\"is_stable\":"));
    output.print(status.runtime.stability.isStable ? F("true") : F("false"));
    output.print(F(",\"total_uptime\":"));
    output.print(status.runtime.stability.totalUptime);
    output.print(F("},"));

    // Logging section
    output.print(F("\"logging\":{"));
    output.print(F("\"serial_enabled\":"));
    output.print(status.runtime.logging.serialEnabled ? F("true") : F("false"));
    output.print(F(",\"log_level\":"));
    writeJSONString(output, status.runtime.logging.currentLogLevel);
    output.print(F(",\"web_serial_enabled\":"));
    output.print(status.runtime.logging.webSerialEnabled ? F("true") : F("false"));
    output.print(F("},"));

    // System health section
    output.print(F("\"health\":{"));
    output.print(F("\"status_update_count\":"));
    output.print(status.runtime.health.statusUpdateCount);
    output.print(F(",\"last_status_update_ms\":"));
    output.print(status.runtime.health.lastStatusUpdateTime);
    output.print(F(",\"system_healthy\":"));
    output.print(status.runtime.health.systemHealthy ? F("true") : F("false"));
    output.print(F(",\"free_heap_trend_bytes_per_minute\":"));
    output.print(status.runtime.health.freeHeapTrendPerMinute);
    output.print(F("}"));

    output.print(F("}"));
    output.print(F("}"));
}

size_t DeviceStatusManager::calculateActualJSONSize() {
    const DeviceStatus& status = deviceStatus;
    // Use counting adapter - no String allocation needed
    CountingPrintAdapter counter;
    buildJSONResponse(counter, status);
    return counter.getCount();
}

void DeviceStatusManager::initializeJSONSizeEstimation() {
    if (sizeEstimationInitialized) return;

    // Calculate actual size by building test JSON with real data
    cachedJSONSize = calculateActualJSONSize();
    sizeEstimationInitialized = true;

    LOG_INFO_SP(F("JSON size estimation initialized: "), true);
    LOG_INFOLN_SP(String(cachedJSONSize), false);
}

size_t DeviceStatusManager::getEstimatedJSONSize() {
    if (!sizeEstimationInitialized) {
        // This should never happen if web setup is called properly
        LOG_ERRORLN(F("JSON size estimation not initialized - this is a bug!"));
        return 0; // Force failure to catch the bug
    }
    return cachedJSONSize;
}

void DeviceStatusManager::recalculateJSONSizeEstimation() {
    cachedJSONSize = calculateActualJSONSize();
    sizeEstimationInitialized = true;

    LOG_INFO_SP(F("JSON size estimation recalculated: "), true);
    LOG_INFOLN_SP(String(cachedJSONSize), false);
}

bool DeviceStatusManager::isJSONSizeEstimationInitialized() {
    return sizeEstimationInitialized;
}

void DeviceStatusManager::resetJSONStreamState(JSONStreamState& state) {
    state.bytesEmitted = 0;
    state.complete = false;
}

size_t DeviceStatusManager::renderJSONChunk(JSONStreamState& state, uint8_t* buffer, size_t maxLen, const DeviceStatus& status) {
    if (state.complete || maxLen == 0) {
        return 0;
    }

    JSONChunkPrint output(buffer, maxLen, state.bytesEmitted);
    buildJSONResponse(output, status);
    state.bytesEmitted += output.bytesWritten();
    state.complete = state.bytesEmitted >= output.totalBytes();
    return output.bytesWritten();
}

#endif // ENABLE_WEB_INTERFACE
