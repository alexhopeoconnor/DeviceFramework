#ifndef CACHED_ENDPOINT_TESTER_H
#define CACHED_ENDPOINT_TESTER_H

#include <Arduino.h>
#include <DeviceFrameworkPlatform.h>  // Platform abstraction (includes HTTP client headers)
#include "HTTPTestManager.h"

/**
 * Simplified cached endpoint testing - fetches endpoint, streams chunks, verifies content
 * This avoids OOM issues by streaming content in small chunks
 */

// Configuration (can be overridden via -D at compile time)
#ifndef CACHED_ENDPOINT_TESTER_CHUNK_SIZE
#define CACHED_ENDPOINT_TESTER_CHUNK_SIZE 256
#endif

#ifndef CACHED_ENDPOINT_TESTER_JSON_DOC_CAPACITY
#define CACHED_ENDPOINT_TESTER_JSON_DOC_CAPACITY 1024
#endif

#ifndef CACHED_ENDPOINT_TESTER_OVERLAP_BYTES
#define CACHED_ENDPOINT_TESTER_OVERLAP_BYTES 128
#endif

class CachedEndpointTester {
public:
    struct TestResult {
        bool success;
        String errorMessage;
        int chunksProcessed;
        long totalBytes;
    };

    static TestResult testEndpoint(
        const String& fetcherUrl,
        const String& deviceIp,
        const String& endpoint,
        int expectedStatusCode,
        const String searchTerms[],
        int searchTermCount);

private:
    static bool streamAndSearch(
        const String& fetcherUrl,
        const String& endpointHash,
        const String searchTerms[],
        int searchTermCount,
        int& chunksProcessed,
        long& totalBytes);
};

#endif
