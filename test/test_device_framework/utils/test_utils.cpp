#include "test_utils.h"
#include "Utils/TimeUtils.h"
#include <WiFiClient.h>
#include <DeviceFrameworkPlatform.h>  // Platform abstraction (includes HTTP client headers)
#include <DeviceFrameworkDebug.h>

// Simple HTTP request utility function for basic health checks
bool testHttpEndpoint(const String& url, int expectedCode, const String& expectedContent, const String& testName, int timeoutMs) {
    Serial.print("[TEST]   Testing ");
    Serial.print(testName);
    Serial.println("...");

    // Get shared HTTP client
    HTTPClient* client = HTTPTestManager::getClient();
    if (!client) {
        Serial.print("[TEST]   ");
        Serial.print(testName);
        Serial.println(" failed - could not get HTTP client");
        return false;
    }

    // Begin request
    client->begin(*HTTPTestManager::getSharedClient(), url);
    client->setTimeout(timeoutMs);

    // Make request
    int httpCode = client->GET();

    // Get response for content checking
    String response = "";
    if (expectedContent.length() > 0 && httpCode == expectedCode) {
        response = client->getString();
    }

    // End request
    client->end();

    // Check results
    bool success = false;
    if (httpCode == expectedCode) {
        if (expectedContent.length() == 0 || response.indexOf(expectedContent) >= 0) {
            success = true;
        }
    }

    if (success) {
        Serial.print("[TEST]   ");
        Serial.print(testName);
        Serial.println(" successful!");
        return true;
    } else {
        Serial.print("[TEST]   ");
        Serial.print(testName);
        Serial.print(" failed - code: ");
        Serial.print(httpCode);
        if (!success) {
            Serial.print(" (content not found)");
        }
        Serial.println();
        return false;
    }
}

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
