#ifndef DEVICEFRAMEWORK_DEBUG_H
#define DEVICEFRAMEWORK_DEBUG_H

#include <Arduino.h>
#include <IPAddress.h>
#include <type_traits>
#include "Utils/DeviceFrameworkFlashStringUtils.h"
#include <DeviceFrameworkPlatform.h>  // Platform abstraction for heap stats

#ifdef ENABLE_WEB_INTERFACE
// Forward declaration for DeviceFrameworkWebSerial
class DeviceFrameworkWebSerial;

// Function to send debug output to WebSocket (called from WebInterface)
void sendDebugToWebSocket(const String& message);
constexpr bool DEVICEFRAMEWORK_WEB_LOGGING_ENABLED = true;
#else
// Keep the logging API link-safe when the optional web interface is omitted.
inline void sendDebugToWebSocket(const String&) {}
constexpr bool DEVICEFRAMEWORK_WEB_LOGGING_ENABLED = false;
#endif

// Log levels enum
enum class LogLevel {
    OFF = 0,
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    VERBOSE = 4
};

// Default log level if not specified via build flag
#ifndef LOG_LEVEL
    #define LOG_LEVEL 0
#endif

// Convert LOG_LEVEL build flag to LogLevel enum (compile-time default)
constexpr LogLevel DEFAULT_LOG_LEVEL = static_cast<LogLevel>(LOG_LEVEL);

// Runtime log level (can be changed at runtime, initialized from DEFAULT_LOG_LEVEL)
extern LogLevel currentLogLevel;

// Helper function to check if a log level should be printed
// Changed to use runtime variable for dynamic log level control
inline bool shouldLog(LogLevel level) {
    return static_cast<int>(level) <= static_cast<int>(currentLogLevel) && currentLogLevel != LogLevel::OFF;
}

// Helper functions for log level conversion (defined in DeviceFrameworkDebug.cpp)
const char* logLevelToString(LogLevel level);
LogLevel stringToLogLevel(const char* str);

// Initialize runtime log level from saved parameter or compile-time default
void applyLogLevel(const char* logLevelValue);

// Apply default log level (from build flag/constant) for early setup phases
void applyDefaultLogLevel();

// Serial management methods
void startSerial(unsigned long baudRate = 0);
void stopSerial();
bool isSerialActive();
void markSerialAsInitialized(); // Call this if Serial was initialized externally

// Debug-specific helper functions for WebSocket message building
namespace DeviceFrameworkDebug {
    /**
     * Helper function to safely convert content to String for fallback cases
     */
    // Template specialization for IPAddress
    template<typename T>
    String convertContentToString(const T& content) {
        return String(content);
    }

    // Specialization for IPAddress
    template<>
    inline String convertContentToString<IPAddress>(const IPAddress& content) {
        return content.toString();
    }

    /**
     * Safely allocate and build a debug message buffer
     * @param prefix Flash string prefix (e.g., F("[ERROR] "))
     * @param content The content to append
     * @param includePrefix Whether to include the prefix
     * @param includeNewline Whether to add a newline at the end
     * @return String ready for WebSocket transmission
     */
    template<typename T>
    String buildDebugMessageBuffer(const __FlashStringHelper* prefix, const T& content, bool includePrefix, bool includeNewline) {
        size_t prefixLen = includePrefix ? strlen_P(reinterpret_cast<const char*>(prefix)) : 0;
        size_t contentLen = DeviceFrameworkFlashStringUtils::calculateLength(content);
        size_t newlineLen = includeNewline ? 1 : 0;
        size_t totalLen = prefixLen + contentLen + newlineLen + 1; // +1 for null terminator

        // Safety checks
        if (totalLen > 512 || totalLen == 0) {
            // Fall back to String approach for safety
            String msg;
            if (includePrefix) msg += prefix;
            msg += convertContentToString(content);
            if (includeNewline) msg += F("\n");
            return msg;
        }

        // Check available heap
        if (ESP.getFreeHeap() < totalLen + 1024) {  // Leave 1KB safety margin
            // Not enough memory - use String approach
            String msg;
            if (includePrefix) msg += prefix;
            msg += convertContentToString(content);
            if (includeNewline) msg += F("\n");
            return msg;
        }

        // Safe to allocate
        char* buffer = (char*)malloc(totalLen);
        if (buffer == nullptr) {
            // malloc failed - fall back to String approach
            String msg;
            if (includePrefix) msg += prefix;
            msg += convertContentToString(content);
            if (includeNewline) msg += F("\n");
            return msg;
        }

        // Build the message
        char* ptr = buffer;
        if (includePrefix) {
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, prefix);
        }
        ptr = DeviceFrameworkFlashStringUtils::appendToBuffer(ptr, content);
        if (includeNewline) {
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F("\n"));
        }

        String result(buffer);
        free(buffer);
        return result;
    }
}

// Enhanced debug macros with log levels and optional WebSocket output
#define LOG_ERROR_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::ERROR) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[ERROR] ")); \
        } \
        Serial.print(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[ERROR] "), x, includePrefix, false); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

#define LOG_ERRORLN_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::ERROR) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[ERROR] ")); \
        } \
        Serial.println(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[ERROR] "), x, includePrefix, true); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

#define LOG_WARN_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::WARN) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[WARN] ")); \
        } \
        Serial.print(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[WARN] "), x, includePrefix, false); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

#define LOG_WARNLN_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::WARN) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[WARN] ")); \
        } \
        Serial.println(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[WARN] "), x, includePrefix, true); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

#define LOG_INFO_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::INFO) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[INFO] ")); \
        } \
        Serial.print(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[INFO] "), x, includePrefix, false); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

#define LOG_INFOLN_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::INFO) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[INFO] ")); \
        } \
        Serial.println(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[INFO] "), x, includePrefix, true); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

#define LOG_DEBUG_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::VERBOSE) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[DEBUG] ")); \
        } \
        Serial.print(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[DEBUG] "), x, includePrefix, false); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

#define LOG_DEBUGLN_SP(x, includePrefix) do { \
    if (shouldLog(LogLevel::VERBOSE) && isSerialActive()) { \
        if (includePrefix) { \
            Serial.print(F("[DEBUG] ")); \
        } \
        Serial.println(x); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            String msg = DeviceFrameworkDebug::buildDebugMessageBuffer(F("[DEBUG] "), x, includePrefix, true); \
            sendDebugToWebSocket(msg); \
        } \
    } \
} while(0)

// Backward compatibility macros (default to DEBUG level)
#define DEBUG_PRINT(x) LOG_DEBUG_SP(x, true)
#define DEBUG_PRINTLN(x) LOG_DEBUGLN_SP(x, true)

// Backward compatibility shell definitions (default to include prefix)
#define LOG_ERROR(x) LOG_ERROR_SP(x, true)
#define LOG_ERRORLN(x) LOG_ERRORLN_SP(x, true)
#define LOG_WARN(x) LOG_WARN_SP(x, true)
#define LOG_WARNLN(x) LOG_WARNLN_SP(x, true)
#define LOG_INFO(x) LOG_INFO_SP(x, true)
#define LOG_INFOLN(x) LOG_INFOLN_SP(x, true)
#define LOG_DEBUG(x) LOG_DEBUG_SP(x, true)
#define LOG_DEBUGLN(x) LOG_DEBUGLN_SP(x, true)

// Memory statistics structure - Always available for core functionality
struct MemoryStats {
    uint32_t freeHeap;
    uint32_t maxBlock;
    uint8_t fragmentation;

    MemoryStats() : freeHeap(0), maxBlock(0), fragmentation(0) {}
    MemoryStats(uint32_t free, uint32_t max, uint8_t frag)
        : freeHeap(free), maxBlock(max), fragmentation(frag) {}
};

// Get current memory statistics - Always available for core functionality
MemoryStats getMemoryStats();

// Memory logging functionality (enabled with ENABLE_MEMORY_LOGGING build flag)
#ifdef ENABLE_MEMORY_LOGGING

// Log memory statistics with optional context
void logMemoryStats(const char* context = nullptr, LogLevel level = LogLevel::INFO);

// Memory logging macros for before/after operations
#define LOG_MEMORY_BEFORE(context) do { \
    if (shouldLog(LogLevel::INFO) && isSerialActive()) { \
        Serial.print(F("[MEMORY BEFORE] ")); \
        Serial.print(context); \
        Serial.print(F(" - ")); \
        uint32_t freeHeap, maxBlock; \
        uint8_t fragmentation; \
        DF_GET_HEAP_STATS(freeHeap, maxBlock, fragmentation); \
        Serial.print(F("Free: ")); \
        Serial.print(freeHeap); \
        Serial.print(F(" bytes, Max Block: ")); \
        Serial.print(maxBlock); \
        Serial.print(F(" bytes, Fragmentation: ")); \
        Serial.print(fragmentation); \
        Serial.println(F("%")); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            char buffer[128]; \
            char* ptr = buffer; \
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F("[MEMORY BEFORE] ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendString(ptr, context); \
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F(" - Free: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendUIntWithSuffix(ptr, freeHeap, F(" bytes, Max Block: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendUIntWithSuffix(ptr, maxBlock, F(" bytes, Fragmentation: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendUIntWithSuffix(ptr, fragmentation, F("%\n")); \
            sendDebugToWebSocket(String(buffer)); \
        } \
    } \
} while(0)

#define LOG_MEMORY_AFTER(context) do { \
    if (shouldLog(LogLevel::INFO) && isSerialActive()) { \
        Serial.print(F("[MEMORY AFTER] ")); \
        Serial.print(context); \
        Serial.print(F(" - ")); \
        uint32_t freeHeap, maxBlock; \
        uint8_t fragmentation; \
        DF_GET_HEAP_STATS(freeHeap, maxBlock, fragmentation); \
        Serial.print(F("Free: ")); \
        Serial.print(freeHeap); \
        Serial.print(F(" bytes, Max Block: ")); \
        Serial.print(maxBlock); \
        Serial.print(F(" bytes, Fragmentation: ")); \
        Serial.print(fragmentation); \
        Serial.println(F("%")); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            char buffer[128]; \
            char* ptr = buffer; \
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F("[MEMORY AFTER] ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendString(ptr, context); \
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F(" - Free: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendUIntWithSuffix(ptr, freeHeap, F(" bytes, Max Block: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendUIntWithSuffix(ptr, maxBlock, F(" bytes, Fragmentation: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendUIntWithSuffix(ptr, fragmentation, F("%\n")); \
            sendDebugToWebSocket(String(buffer)); \
        } \
    } \
} while(0)

#define LOG_MEMORY_DELTA(beforeStats, context) do { \
    if (shouldLog(LogLevel::INFO) && isSerialActive()) { \
        uint32_t freeHeap, maxBlock; \
        uint8_t fragmentation; \
        DF_GET_HEAP_STATS(freeHeap, maxBlock, fragmentation); \
        int32_t deltaFree = (int32_t)freeHeap - (int32_t)beforeStats.freeHeap; \
        int32_t deltaMaxBlock = (int32_t)maxBlock - (int32_t)beforeStats.maxBlock; \
        int32_t deltaFragmentation = (int32_t)fragmentation - (int32_t)beforeStats.fragmentation; \
        Serial.print(F("[MEMORY DELTA] ")); \
        Serial.print(context); \
        Serial.print(F(" - Free: ")); \
        Serial.print(deltaFree); \
        Serial.print(F(" bytes, Max Block: ")); \
        Serial.print(deltaMaxBlock); \
        Serial.print(F(" bytes, Fragmentation: ")); \
        Serial.print(deltaFragmentation); \
        Serial.println(F("%")); \
        if (DEVICEFRAMEWORK_WEB_LOGGING_ENABLED) { \
            char buffer[128]; \
            char* ptr = buffer; \
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F("[MEMORY DELTA] ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendString(ptr, context); \
            ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F(" - Free: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendIntWithSuffix(ptr, deltaFree, F(" bytes, Max Block: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendIntWithSuffix(ptr, deltaMaxBlock, F(" bytes, Fragmentation: ")); \
            ptr = DeviceFrameworkFlashStringUtils::appendIntWithSuffix(ptr, deltaFragmentation, F("%\n")); \
            sendDebugToWebSocket(String(buffer)); \
        } \
    } \
} while(0)

#else
// Memory logging disabled - define empty macros
#define LOG_MEMORY_BEFORE(context) do {} while(0)
#define LOG_MEMORY_AFTER(context) do {} while(0)
#define LOG_MEMORY_DELTA(beforeStats, context) do {} while(0)

#endif // ENABLE_MEMORY_LOGGING

#endif // DEVICEFRAMEWORK_DEBUG_H
