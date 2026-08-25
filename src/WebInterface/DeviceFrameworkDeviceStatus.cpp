#ifdef ENABLE_WEB_INTERFACE
#include "DeviceFrameworkDeviceStatus.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../Configuration/DeviceFrameworkIdentity.h"
#include "../WiFi/DeviceFrameworkWiFi.h"
#include "../MQTT/DeviceFrameworkMQTT.h"
#include "../Utils/TimeUtils.h"
#include "../Utils/PrintAdapters.h"
#include "../Storage/DeviceFrameworkRTC.h"
#include <DeviceFrameworkPlatform.h>

namespace {

bool prepareNextJSONFragment(DeviceStatusManager::JSONStreamState& state, const DeviceStatus& status) {
    switch (state.fragmentIndex) {
        case 0:
            state.pendingFragment = F("{");
            break;
        case 1:
            state.pendingFragment =
                String(F("\"hardware\":{")) +
                F("\"max_memory\":") + String(status.hardware.maxMemory) +
                F(",\"chip_id\":\"") + status.hardware.chipId +
                F("\",\"flash_size\":") + String(status.hardware.flashSize) +
                F(",\"flash_speed\":") + String(status.hardware.flashSpeed) +
                F(",\"cpu_freq\":") + String(status.hardware.cpuFreq) +
                F(",\"sketch_size\":") + String(status.hardware.sketchSize) +
                F(",\"free_sketch_space\":") + String(status.hardware.freeSketchSpace) +
                F(",\"version\":\"") + status.hardware.version +
                F("\"},");
            break;
        case 2:
            state.pendingFragment = F("\"runtime\":{");
            break;
        case 3:
            state.pendingFragment =
                String(F("\"wifi\":{")) +
                F("\"connected\":") + String(status.runtime.wifi.connected ? F("true") : F("false")) +
                F(",\"ssid\":\"") + status.runtime.wifi.ssid +
                F("\",\"ip\":\"") + status.runtime.wifi.ip +
                F("\",\"gateway\":\"") + status.runtime.wifi.gateway +
                F("\",\"subnet\":\"") + status.runtime.wifi.subnet +
                F("\",\"dns\":\"") + status.runtime.wifi.dns +
                F("\",\"mac\":\"") + status.runtime.wifi.mac +
                F("\",\"rssi\":") + String(status.runtime.wifi.rssi) +
                F(",\"channel\":") + String(status.runtime.wifi.channel) +
                F(",\"bssid\":\"") + status.runtime.wifi.bssid +
                F("\",\"connection_duration\":") + String(status.runtime.wifi.connectionDuration) +
                F(",\"total_disconnections\":") + String(status.runtime.wifi.totalDisconnections) +
                F(",\"last_disconnection_time\":") + String(status.runtime.wifi.lastDisconnectionTime) +
                F(",\"is_stable\":") + String(status.runtime.wifi.isStable ? F("true") : F("false")) +
                F("},");
            break;
        case 4:
            state.pendingFragment =
                String(F("\"mqtt\":{")) +
                F("\"connected\":") + String(status.runtime.mqtt.connected ? F("true") : F("false")) +
                F(",\"broker\":\"") + status.runtime.mqtt.broker +
                F("\",\"port\":") + String(status.runtime.mqtt.port) +
                F(",\"user\":\"") + status.runtime.mqtt.user +
                F("\",\"connection_duration\":") + String(status.runtime.mqtt.connectionDuration) +
                F(",\"total_disconnections\":") + String(status.runtime.mqtt.totalDisconnections) +
                F(",\"last_disconnection_time\":") + String(status.runtime.mqtt.lastDisconnectionTime) +
                F(",\"is_stable\":") + String(status.runtime.mqtt.isStable ? F("true") : F("false")) +
                F("},");
            break;
        case 5:
            state.pendingFragment =
                String(F("\"device\":{")) +
                F("\"name\":\"") + status.runtime.device.deviceName +
                F("\",\"uptime\":") + String(status.runtime.device.uptime) +
                F("},");
            break;
        case 6:
            state.pendingFragment =
                String(F("\"memory\":{")) +
                F("\"free\":") + String(status.runtime.memory.freeMemory) +
                F(",\"max_block\":") + String(status.runtime.memory.maxBlockSize) +
                F(",\"fragmentation\":") + String(status.runtime.memory.fragmentation) +
                F(",\"delta\":") + String(status.runtime.memory.memoryDelta) +
                F(",\"peak_usage\":") + String(status.runtime.memory.peakMemoryUsage) +
                F(",\"lowest_usage\":") + String(status.runtime.memory.lowestMemoryUsage) +
                F("},");
            break;
        case 7:
            state.pendingFragment =
                String(F("\"stability\":{")) +
                F("\"reset_count\":") + String(status.runtime.stability.resetCount) +
                F(",\"time_since_reset\":") + String(status.runtime.stability.timeSinceLastReset) +
                F(",\"is_stable\":") + String(status.runtime.stability.isStable ? F("true") : F("false")) +
                F(",\"total_uptime\":") + String(status.runtime.stability.totalUptime) +
                F("},");
            break;
        case 8:
            state.pendingFragment =
                String(F("\"logging\":{")) +
                F("\"serial_enabled\":") + String(status.runtime.logging.serialEnabled ? F("true") : F("false")) +
                F(",\"log_level\":\"") + status.runtime.logging.currentLogLevel +
                F("\",\"web_serial_enabled\":") + String(status.runtime.logging.webSerialEnabled ? F("true") : F("false")) +
                F("},");
            break;
        case 9:
            state.pendingFragment =
                String(F("\"health\":{")) +
                F("\"loop_count\":") + String(status.runtime.health.loopCount) +
                F(",\"last_loop_time\":") + String(status.runtime.health.lastLoopTime) +
                F(",\"system_healthy\":") + String(status.runtime.health.systemHealthy ? F("true") : F("false")) +
                F(",\"free_heap_trend\":") + String(status.runtime.health.freeHeapTrend) +
                F("}");
            break;
        case 10:
            state.pendingFragment = F("}");
            break;
        case 11:
            state.pendingFragment = F("}");
            break;
        default:
            state.complete = true;
            state.pendingFragment = "";
            state.pendingOffset = 0;
            return false;
    }

    state.pendingOffset = 0;
    state.fragmentIndex++;
    return true;
}

size_t copyPendingJSONFragment(DeviceStatusManager::JSONStreamState& state, uint8_t* buffer, size_t maxLen, size_t offset) {
    size_t available = state.pendingFragment.length() - state.pendingOffset;
    size_t remaining = maxLen - offset;
    size_t toWrite = (available < remaining) ? available : remaining;

    if (toWrite == 0) {
        return offset;
    }

    memcpy(buffer + offset, state.pendingFragment.c_str() + state.pendingOffset, toWrite);
    state.pendingOffset += toWrite;

    if (state.pendingOffset >= state.pendingFragment.length()) {
        state.pendingFragment = "";
        state.pendingOffset = 0;
    }

    return offset + toWrite;
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
unsigned long DeviceStatusManager::lastLoopTime = 0;
uint32_t DeviceStatusManager::loopCount = 0;
int32_t DeviceStatusManager::freeHeapTrend = 0;
uint32_t DeviceStatusManager::peakMemoryUsage = 0;
uint32_t DeviceStatusManager::lowestMemoryUsage = 0;
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
    lastLoopTime = millis();
    loopCount = 0;
    wifiDisconnectionCount = 0;
    mqttDisconnectionCount = 0;
    peakMemoryUsage = 0;
    lowestMemoryUsage = 0;

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

    loopCount++;
    lastLoopTime = currentTime;

    // Simple heap trend calculation (last 10 measurements)
    static uint32_t heapHistory[10];
    static uint8_t heapIndex = 0;

    uint32_t currentHeap = ESP.getFreeHeap();
    heapHistory[heapIndex] = currentHeap;
    heapIndex = (heapIndex + 1) % 10;

    // Calculate trend (positive = increasing, negative = decreasing)
    int32_t trend = 0;
    for (int i = 1; i < 10; i++) {
        trend += (int32_t)heapHistory[i] - (int32_t)heapHistory[i-1];
    }
    freeHeapTrend = trend / 10;

    // Track peak/lowest memory usage
    if (currentHeap > peakMemoryUsage) {
        peakMemoryUsage = currentHeap;
    }
    if (currentHeap < lowestMemoryUsage || lowestMemoryUsage == 0) {
        lowestMemoryUsage = currentHeap;
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
        deviceStatus.runtime.wifi.ssid = WiFi.SSID();
        deviceStatus.runtime.wifi.ip = WiFi.localIP().toString();
        deviceStatus.runtime.wifi.gateway = WiFi.gatewayIP().toString();
        deviceStatus.runtime.wifi.subnet = WiFi.subnetMask().toString();
        deviceStatus.runtime.wifi.dns = WiFi.dnsIP().toString();
        deviceStatus.runtime.wifi.mac = WiFi.macAddress();
        deviceStatus.runtime.wifi.rssi = WiFi.RSSI();
        deviceStatus.runtime.wifi.channel = WiFi.channel();
        deviceStatus.runtime.wifi.bssid = WiFi.BSSIDstr();

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
        deviceStatus.runtime.mqtt.broker = String(DeviceFrameworkParameters::getMqttServer());
        deviceStatus.runtime.mqtt.port = DeviceFrameworkParameters::getMqttPort();
        deviceStatus.runtime.mqtt.user = String(DeviceFrameworkParameters::getMqttUser());

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
    deviceStatus.runtime.device.deviceName = String(DeviceFrameworkParameters::getDeviceName());
    deviceStatus.runtime.device.uptime = TimeUtils::safeTimeDifference(now, bootTime) / 1000;
    deviceStatus.runtime.device.configMode = DeviceFrameworkWiFi::isInConfigMode();

    // Enhanced memory info
    MemoryStats currentStats = getMemoryStats();
    deviceStatus.runtime.memory.freeMemory = currentStats.freeHeap;
    deviceStatus.runtime.memory.maxBlockSize = currentStats.maxBlock;
    deviceStatus.runtime.memory.fragmentation = currentStats.fragmentation;

    // Calculate memory delta using TimeUtils
    if (deviceStatus.lastMemoryCheck > 0) {
        deviceStatus.runtime.memory.memoryDelta = (int32_t)currentStats.freeHeap - (int32_t)lastMemoryStats.freeHeap;
    }
    lastMemoryStats = currentStats;
    deviceStatus.lastMemoryCheck = now;

    // Runtime memory tracking (RAM only)
    deviceStatus.runtime.memory.peakMemoryUsage = peakMemoryUsage;
    deviceStatus.runtime.memory.lowestMemoryUsage = lowestMemoryUsage;

    // Logging info
    deviceStatus.runtime.logging.serialEnabled = isSerialActive();
    deviceStatus.runtime.logging.currentLogLevel = logLevelToString(currentLogLevel);
    deviceStatus.runtime.logging.webSerialEnabled = true; // Will be updated by web interface

    // System health
    deviceStatus.runtime.health.loopCount = loopCount;
    deviceStatus.runtime.health.lastLoopTime = lastLoopTime;
    deviceStatus.runtime.health.freeHeapTrend = freeHeapTrend;
    deviceStatus.runtime.health.systemHealthy = (freeHeapTrend > -1000 && // Not losing more than 1KB per loop
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
    output.print(F(",\"chip_id\":\""));
    output.print(status.hardware.chipId);
    output.print(F("\",\"flash_size\":"));
    output.print(status.hardware.flashSize);
    output.print(F(",\"flash_speed\":"));
    output.print(status.hardware.flashSpeed);
    output.print(F(",\"cpu_freq\":"));
    output.print(status.hardware.cpuFreq);
    output.print(F(",\"sketch_size\":"));
    output.print(status.hardware.sketchSize);
    output.print(F(",\"free_sketch_space\":"));
    output.print(status.hardware.freeSketchSpace);
    output.print(F(",\"version\":\""));
    output.print(status.hardware.version);
    output.print(F("\"},"));

    // Runtime info (dynamic)
    output.print(F("\"runtime\":{"));

    // WiFi stability section
    output.print(F("\"wifi\":{"));
    output.print(F("\"connected\":"));
    output.print(status.runtime.wifi.connected ? F("true") : F("false"));
    output.print(F(",\"ssid\":\""));
    output.print(status.runtime.wifi.ssid);
    output.print(F("\",\"ip\":\""));
    output.print(status.runtime.wifi.ip);
    output.print(F("\",\"gateway\":\""));
    output.print(status.runtime.wifi.gateway);
    output.print(F("\",\"subnet\":\""));
    output.print(status.runtime.wifi.subnet);
    output.print(F("\",\"dns\":\""));
    output.print(status.runtime.wifi.dns);
    output.print(F("\",\"mac\":\""));
    output.print(status.runtime.wifi.mac);
    output.print(F("\",\"rssi\":"));
    output.print(status.runtime.wifi.rssi);
    output.print(F(",\"channel\":"));
    output.print(status.runtime.wifi.channel);
    output.print(F(",\"bssid\":\""));
    output.print(status.runtime.wifi.bssid);
    output.print(F("\",\"connection_duration\":"));
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
    output.print(F(",\"broker\":\""));
    output.print(status.runtime.mqtt.broker);
    output.print(F("\",\"port\":"));
    output.print(status.runtime.mqtt.port);
    output.print(F(",\"user\":\""));
    output.print(status.runtime.mqtt.user);
    output.print(F("\",\"connection_duration\":"));
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
    output.print(F("\"name\":\""));
    output.print(status.runtime.device.deviceName);
    output.print(F("\",\"uptime\":"));
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
    output.print(F(",\"delta\":"));
    output.print(status.runtime.memory.memoryDelta);
    output.print(F(",\"peak_usage\":"));
    output.print(status.runtime.memory.peakMemoryUsage);
    output.print(F(",\"lowest_usage\":"));
    output.print(status.runtime.memory.lowestMemoryUsage);
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
    output.print(F(",\"log_level\":\""));
    output.print(status.runtime.logging.currentLogLevel);
    output.print(F("\",\"web_serial_enabled\":"));
    output.print(status.runtime.logging.webSerialEnabled ? F("true") : F("false"));
    output.print(F("},"));

    // System health section
    output.print(F("\"health\":{"));
    output.print(F("\"loop_count\":"));
    output.print(status.runtime.health.loopCount);
    output.print(F(",\"last_loop_time\":"));
    output.print(status.runtime.health.lastLoopTime);
    output.print(F(",\"system_healthy\":"));
    output.print(status.runtime.health.systemHealthy ? F("true") : F("false"));
    output.print(F(",\"free_heap_trend\":"));
    output.print(status.runtime.health.freeHeapTrend);
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
    state.fragmentIndex = 0;
    state.pendingFragment = "";
    state.pendingOffset = 0;
    state.complete = false;
}

size_t DeviceStatusManager::renderJSONChunk(JSONStreamState& state, uint8_t* buffer, size_t maxLen, const DeviceStatus& status) {
    if (state.complete || maxLen == 0) {
        return 0;
    }

    size_t written = 0;

    while (written < maxLen && !state.complete) {
        if (state.pendingFragment.length() == 0) {
            if (!prepareNextJSONFragment(state, status)) {
                break;
            }
        }

        written = copyPendingJSONFragment(state, buffer, maxLen, written);
    }

    return written;
}

#endif // ENABLE_WEB_INTERFACE
