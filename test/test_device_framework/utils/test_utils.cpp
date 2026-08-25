#include "test_utils.h"
#include <DeviceFrameworkPlatform.h>
#ifdef ENABLE_MEMORY_LOGGING

// Memory logging utilities for tests
void logTestMemory(const char* testName, const char* phase) {
    uint32_t freeHeap, maxBlock;
    uint8_t fragmentation;
    DF_GET_HEAP_STATS(freeHeap, maxBlock, fragmentation);

    Serial.printf("Free: %u bytes, Max Block: %u bytes, Fragmentation: %u%%\n",
                  freeHeap, maxBlock, fragmentation);
}

MemoryStats getTestMemoryStats() {
    uint32_t freeHeap, maxBlock;
    uint8_t fragmentation;
    DF_GET_HEAP_STATS(freeHeap, maxBlock, fragmentation);
    return MemoryStats(freeHeap, maxBlock, fragmentation);
}

void logMemoryDelta(const MemoryStats& before, const MemoryStats& after, const char* context) {
    int32_t deltaFree = (int32_t)after.freeHeap - (int32_t)before.freeHeap;
    int32_t deltaMaxBlock = (int32_t)after.maxBlock - (int32_t)before.maxBlock;
    int32_t deltaFragmentation = (int32_t)after.fragmentation - (int32_t)before.fragmentation;

    Serial.printf("[TEST] [MEMORY DELTA] %s - Free: %+d bytes, Max Block: %+d bytes, Fragmentation: %+d%%\n",
                  context, deltaFree, deltaMaxBlock, deltaFragmentation);
}

#endif // ENABLE_MEMORY_LOGGING
