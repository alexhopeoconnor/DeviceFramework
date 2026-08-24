#include "DeviceFrameworkDebug.h"
#include "Configuration/DeviceFrameworkParameterRegistry.h"
#include "Configuration/DeviceFrameworkConfig.h"
#include "DeviceFramework.h"
#include <ArduinoHALog.h>

namespace {
ArduinoHALogLevel toArduinoHALogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::ERROR:
            return ArduinoHALogLevel::Error;
        case LogLevel::WARN:
            return ArduinoHALogLevel::Warn;
        case LogLevel::INFO:
            return ArduinoHALogLevel::Info;
        case LogLevel::VERBOSE:
            return ArduinoHALogLevel::Trace;
        case LogLevel::OFF:
        default:
            return ArduinoHALogLevel::Info;
    }
}

void syncArduinoHALogging(LogLevel level) {
    if (level == LogLevel::OFF) {
        arduinoHASetLogEnabled(false);
        return;
    }

    arduinoHASetLogEnabled(true);
    arduinoHASetLogLevel(toArduinoHALogLevel(level));
}
} // namespace

// Runtime log level (initialized from compile-time default, can be changed at runtime)
LogLevel currentLogLevel = DEFAULT_LOG_LEVEL;

// Helper function to convert LogLevel enum to string
const char* logLevelToString(LogLevel level) {
    switch(level) {
        case LogLevel::OFF: return "Off";
        case LogLevel::ERROR: return "Error";
        case LogLevel::WARN: return "Warn";
        case LogLevel::INFO: return "Info";
        case LogLevel::VERBOSE: return "Verbose";
        default: return "Off";
    }
}

// Helper function to convert string to LogLevel enum
// Returns DEFAULT_LOG_LEVEL if string is empty or invalid (use compile-time setting)
LogLevel stringToLogLevel(const char* str) {
    if (str == nullptr || str[0] == '\0') {
        return DEFAULT_LOG_LEVEL;  // Empty/null means use compile-time default
    }
    // Match the exact case used in the parameter options
    if (strcasecmp(str, "Error") == 0) return LogLevel::ERROR;
    if (strcasecmp(str, "Warn") == 0) return LogLevel::WARN;
    if (strcasecmp(str, "Info") == 0) return LogLevel::INFO;
    if (strcasecmp(str, "Verbose") == 0) return LogLevel::VERBOSE;
    if (strcasecmp(str, "Off") == 0) return LogLevel::OFF;
    return DEFAULT_LOG_LEVEL;  // Invalid value means use compile-time default
}

// Initialize runtime log level from saved parameter or compile-time default
void applyLogLevel(const char* logLevelValue) {
    LogLevel newLogLevel;

    if (logLevelValue != nullptr && logLevelValue[0] != '\0') {
        // User has explicitly set a log level - use it
        newLogLevel = stringToLogLevel(logLevelValue);
    } else {
        // No user override - use compile-time default
        newLogLevel = DEFAULT_LOG_LEVEL;
    }

    // Update the log level
    currentLogLevel = newLogLevel;

    // Manage Serial based on log level
    if (currentLogLevel == LogLevel::OFF) {
        // Log level is OFF - stop Serial to save resources
        stopSerial();
    } else {
        // Log level is not OFF - ensure Serial is started
        startSerial();
    }

    syncArduinoHALogging(currentLogLevel);

    // Log the change (only if Serial is active and we're not in early setup)
    if (isSerialActive()) {
        if (logLevelValue != nullptr && logLevelValue[0] != '\0') {
            LOG_INFO_SP(F("Log level set by user: "), true);
            LOG_INFOLN_SP(logLevelValue, false);
        } else {
            LOG_INFO_SP(F("Log level using compile-time default: "), true);
            LOG_INFOLN_SP(logLevelToString(DEFAULT_LOG_LEVEL), false);
        }
    }
}

// Apply default log level (from build flag/constant) for early setup phases
void applyDefaultLogLevel() {
    // Apply compile-time default log level
    currentLogLevel = DEFAULT_LOG_LEVEL;

    // Only start Serial if log level is not OFF
    if (currentLogLevel != LogLevel::OFF) {
        startSerial();
    }

    syncArduinoHALogging(currentLogLevel);
}

// Get current memory statistics - Always available for core functionality
MemoryStats getMemoryStats() {
    uint32_t freeHeap, maxBlock;
    uint8_t fragmentation;
    DF_GET_HEAP_STATS(freeHeap, maxBlock, fragmentation);
    return MemoryStats(freeHeap, maxBlock, fragmentation);
}

// Memory logging functionality (enabled with ENABLE_MEMORY_LOGGING build flag)
#ifdef ENABLE_MEMORY_LOGGING

// Log memory statistics with optional context
void logMemoryStats(const char* context, LogLevel level) {
    if (!shouldLog(level)) return;

    MemoryStats stats = getMemoryStats();

    if (context) {
        LOG_INFO_SP(F("Memory Stats ["), true);
        LOG_INFO_SP(context, false);
        LOG_INFO_SP(F("] - "), false);
    } else {
        LOG_INFO_SP(F("Memory Stats - "), true);
    }

    Serial.printf("Free: %u bytes, Max Block: %u bytes, Fragmentation: %u%%\n",
                  stats.freeHeap, stats.maxBlock, stats.fragmentation);

    if (ENABLE_WEB_INTERFACE) {
        String msg = F("Free: ");
        msg += String(stats.freeHeap);
        msg += F(" bytes, Max Block: ");
        msg += String(stats.maxBlock);
        msg += F(" bytes, Fragmentation: ");
        msg += String(stats.fragmentation);
        msg += F("%\n");
        sendDebugToWebSocket(msg);
    }
}

#endif // ENABLE_MEMORY_LOGGING

// Serial management implementation
static bool serialActive = false;
static bool serialExternallyInitialized = false;
static unsigned long currentBaudRate = 0;

void markSerialAsInitialized() {
    serialExternallyInitialized = true;
    serialActive = true;
}

void startSerial(unsigned long baudRate) {
    if (baudRate == 0) {
        baudRate = getConfigSerialBaudRate();
    }

    // If Serial was marked as externally initialized, don't reinitialize it
    // This prevents crashes on ESP32 when Serial was already initialized at a different baud rate
    if (serialExternallyInitialized) {
        return;
    }

    // Only initialize/reinitialize if we're tracking it and baud rate differs
    // In normal operation (Serial not pre-initialized), this will initialize Serial
    if (!serialActive || currentBaudRate != baudRate) {
        Serial.begin(baudRate);
        serialActive = true;
        currentBaudRate = baudRate;
    }
}

void stopSerial() {
    if (serialActive) {
        // Note: Serial.end() may not be safe in all contexts on ESP8266
        // Instead, we just mark it as inactive and let the system handle it
        serialActive = false;
    }
}

bool isSerialActive() {
    return serialActive;
}
