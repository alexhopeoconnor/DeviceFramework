#ifndef DEVICEFRAMEWORK_DEVICE_STATUS_H
#define DEVICEFRAMEWORK_DEVICE_STATUS_H

#ifdef ENABLE_WEB_INTERFACE
#include <Arduino.h>
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../DeviceFrameworkDebug.h"

// Web-dashboard telemetry cache. This is an internal implementation detail of
// the built-in web interface, not a consumer-sketch extension API. A future
// composition API should expose intentional value/callback types instead.
struct DeviceStatus {
    // Hardware info (truly static)
    struct HardwareInfo {
        uint32_t maxMemory;
        String chipId;
        uint32_t flashSize;
        uint32_t flashSpeed;
        uint32_t cpuFreq;
        uint32_t sketchSize;
        uint32_t freeSketchSpace;
        String version;
    } hardware;

    // Runtime info (changes frequently)
    struct RuntimeInfo {
        // WiFi connection stability (not just connected status)
        struct WiFiStability {
            bool connected;
            char ssid[33]; // IEEE 802.11 SSID: 32 characters plus NUL
            char ip[16];
            char gateway[16];
            char subnet[16];
            char dns[16];
            char mac[18]; // AA:BB:CC:DD:EE:FF
            int32_t rssi;
            uint8_t channel;
            char bssid[18];
            uint32_t connectionDuration; // How long connected (seconds)
            uint32_t totalDisconnections; // Total disconnections since boot
            uint32_t lastDisconnectionTime; // When last disconnected
            bool isStable; // Based on disconnection frequency
        } wifi;

        // MQTT connection stability
        struct MQTTStability {
            bool connected;
            char broker[41]; // Matches the mqttserver parameter limit
            uint16_t port;
            char user[21]; // Matches the mqttuser parameter limit
            uint32_t connectionDuration; // How long connected (seconds)
            uint32_t totalDisconnections; // Total disconnections since boot
            uint32_t lastDisconnectionTime; // When last disconnected
            bool isStable; // Based on disconnection frequency
        } mqtt;

        // Device information
        struct DeviceInfo {
            char deviceName[25]; // Matches the device parameter limit
            uint32_t uptime;
        } device;

        // Enhanced memory info
        struct MemoryInfo {
            uint32_t freeMemory;
            uint32_t maxBlockSize;
            uint8_t fragmentation;
            int32_t freeHeapDelta; // Signed free-heap change since the last status update
            uint32_t highestFreeHeap; // Highest observed free heap since boot
            uint32_t lowestFreeHeap; // Lowest observed free heap since boot
        } memory;

        // Stability info
        struct StabilityInfo {
            uint8_t resetCount;
            uint32_t timeSinceLastReset;
            bool isStable; // Based on reset patterns
            uint32_t totalUptime; // Total uptime across all resets
        } stability;

        // Serial/Logging info
        struct LoggingInfo {
            bool serialEnabled;
            char currentLogLevel[8]; // "Verbose" plus NUL
            bool webSerialEnabled;
        } logging;

        // System health
        struct SystemHealth {
            uint32_t statusUpdateCount;
            uint32_t lastStatusUpdateTime;
            bool systemHealthy;
            int32_t freeHeapTrendPerMinute; // Signed, sampled free-heap trend
        } health;
    } runtime;

    // Timestamps
    unsigned long lastUpdate;
    unsigned long lastMemoryCheck;
};

/**
 * DeviceStatusManager - Manages comprehensive device status information
 * Uses RAM-based state tracking with minimal RTC memory usage
 */
class DeviceStatusManager {
private:
    static DeviceStatus deviceStatus;
    static bool hardwareInitialized;
    static MemoryStats lastMemoryStats;

    // Connection tracking for runtime metrics (RAM only)
    static bool lastWiFiConnected;
    static bool lastMQTTConnected;
    static unsigned long wifiConnectionStartTime;
    static unsigned long mqttConnectionStartTime;
    static uint32_t wifiDisconnectionCount;
    static uint32_t mqttDisconnectionCount;
    static unsigned long lastWifiDisconnectionTime;
    static unsigned long lastMqttDisconnectionTime;

    // Runtime performance tracking (RAM only)
    struct HeapSample {
        uint32_t freeHeap;
        unsigned long timestamp;
    };

    static const uint8_t heapSampleCount = 10;
    static const unsigned long heapSampleInterval = 30000;
    static unsigned long lastStatusUpdateTime;
    static uint32_t statusUpdateCount;
    static int32_t freeHeapTrendPerMinute;
    static uint32_t highestFreeHeap;
    static uint32_t lowestFreeHeap;
    static unsigned long lastHeapSampleTime;
    static HeapSample heapHistory[heapSampleCount];
    static uint8_t heapHistoryLength;
    static uint8_t heapHistoryNext;
    static unsigned long bootTime;

    // Cached estimation data
    static size_t cachedJSONSize;
    static bool sizeEstimationInitialized;

    // Calculate actual JSON size by building it (uses counting adapter, no String allocation)
    static size_t calculateActualJSONSize();

    // Track connection state changes
    static void trackConnectionChanges();

    // Update runtime performance metrics
    static void updateRuntimeMetrics();

public:
    struct JSONStreamState {
        size_t bytesEmitted = 0;
        bool complete = false;
    };

    // Initialize hardware info once
    static void initializeHardwareInfo();

    // Initialize runtime tracking (call once during setup)
    static void initializeRuntimeTracking();

    // Update all runtime status (call this periodically)
    static void updateRuntimeInfo();

    // Check if we need to update dynamic data
    static bool needsUpdate();

    // Get current device status
    static const DeviceStatus& getStatus();

    // Initialize JSON size estimation (call once during web interface setup)
    static void initializeJSONSizeEstimation();

    // Get estimated JSON size (uses cached value)
    static size_t getEstimatedJSONSize();

    // Force recalculation of JSON size estimation (for sketch use)
    static void recalculateJSONSizeEstimation();

    // Check if estimation is initialized
    static bool isJSONSizeEstimationInitialized();

    // Stream JSON directly to Print interface (memory-efficient, no String allocations)
    // Use this for chunked responses to avoid large String allocations
    // This is the preferred method - avoids String concatenation overhead
    static void buildJSONResponse(Print& output, const DeviceStatus& status);

    // Stream JSON directly into chunk buffers without first materializing
    // the entire document into a String.
    static void resetJSONStreamState(JSONStreamState& state);
    static size_t renderJSONChunk(JSONStreamState& state, uint8_t* buffer, size_t maxLen, const DeviceStatus& status);
};

#endif // ENABLE_WEB_INTERFACE
#endif // DEVICEFRAMEWORK_DEVICE_STATUS_H
