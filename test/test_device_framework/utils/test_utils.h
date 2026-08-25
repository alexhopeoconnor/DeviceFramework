#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <Arduino.h>
#include <DeviceFrameworkDebug.h>


// Test structure and state management
using TestFn = void(*)();

struct TestCase {
    const char* name;
    TestFn fn;
    uint16_t line;
};

#define TEST_ENTRY(fn) { #fn, fn, __LINE__ }


// Memory logging utilities for tests
#ifdef ENABLE_MEMORY_LOGGING
void logTestMemory(const char* testName, const char* phase);
MemoryStats getTestMemoryStats();
void logMemoryDelta(const MemoryStats& before, const MemoryStats& after, const char* context);

// Convenience wrappers that handle #ifdef internally
inline MemoryStats getMemoryStatsIfEnabled() {
    return getTestMemoryStats();
}

inline void logDeltaIfEnabled(const MemoryStats& before, const MemoryStats& after, const char* context) {
    logMemoryDelta(before, after, context);
}
#else
#define logTestMemory(testName, phase) do {} while(0)

// Stub implementations for when memory logging is disabled
inline MemoryStats getMemoryStatsIfEnabled() {
    return MemoryStats(0, 0, 0);
}

inline void logDeltaIfEnabled(const MemoryStats&, const MemoryStats&, const char*) {
    // No-op when memory logging is disabled
}
#endif

#endif // TEST_UTILS_H
