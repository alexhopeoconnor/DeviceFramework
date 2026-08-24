#ifndef DEVICEFRAMEWORK_DEVICE_STATUS_H
#define DEVICEFRAMEWORK_DEVICE_STATUS_H

#ifdef ENABLE_WEB_INTERFACE
#include <Arduino.h>
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../DeviceFrameworkDebug.h"

// Enhanced DeviceStatus structure
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
            String ssid;
            String ip;
            String gateway;
            String subnet;
            String dns;
            String mac;
            int32_t rssi;
            uint8_t channel;
            String bssid;
            uint32_t connectionDuration; // How long connected (seconds)
            uint32_t totalDisconnections; // Total disconnections since boot
            uint32_t lastDisconnectionTime; // When last disconnected
            bool isStable; // Based on disconnection frequency
        } wifi;

        // MQTT connection stability
        struct MQTTStability {
            bool connected;
            String broker;
            uint16_t port;
            String user;
            uint32_t connectionDuration; // How long connected (seconds)
            uint32_t totalDisconnections; // Total disconnections since boot
            uint32_t lastDisconnectionTime; // When last disconnected
            bool isStable; // Based on disconnection frequency
        } mqtt;

        // Device information
        struct DeviceInfo {
            String deviceName;
            uint32_t uptime;
            bool configMode;
        } device;

        // Enhanced memory info
        struct MemoryInfo {
            uint32_t freeMemory;
            uint32_t maxBlockSize;
            uint8_t fragmentation;
            uint32_t memoryDelta; // Change since last check
            uint32_t peakMemoryUsage; // Highest memory usage since boot
            uint32_t lowestMemoryUsage; // Lowest memory usage since boot
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
            String currentLogLevel;
            bool webSerialEnabled;
        } logging;

        // System health
        struct SystemHealth {
            uint32_t loopCount;
            uint32_t lastLoopTime;
            bool systemHealthy;
            uint32_t freeHeapTrend; // Trend over time
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
    static unsigned long lastLoopTime;
    static uint32_t loopCount;
    static int32_t freeHeapTrend;
    static uint32_t peakMemoryUsage;
    static uint32_t lowestMemoryUsage;
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
        uint8_t fragmentIndex = 0;
        String pendingFragment;
        size_t pendingOffset = 0;
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
