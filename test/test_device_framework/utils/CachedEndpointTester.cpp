#include "CachedEndpointTester.h"
#include <ArduinoJson.h>

CachedEndpointTester::TestResult CachedEndpointTester::testEndpoint(
    const String& fetcherUrl,
    const String& deviceIp,
    const String& endpoint,
    int expectedStatusCode,
    const String searchTerms[],
    int searchTermCount)
{
    TestResult result = { false, "", 0, 0 };

    HTTPClient* client = HTTPTestManager::getClient();
    if (!client) {
        result.errorMessage = "Failed to get HTTP client";
        return result;
    }

    Serial.print("[TEST]   Testing endpoint: ");
    Serial.println(endpoint);

    // Step 1: Schedule fetch (returns immediately with jobId)
    String fetchUrl = fetcherUrl + "/api/fetch";
    client->begin(*HTTPTestManager::getSharedClient(), fetchUrl);
    client->addHeader("Content-Type", "application/json");

    String fetchBody = "{\"deviceIp\":\"" + deviceIp + "\",\"endpoint\":\"" + endpoint + "\"}";
    int fetchCode = client->POST(fetchBody);

    Serial.print("[TEST]   Fetch request returned code: ");
    Serial.println(fetchCode);

    if (fetchCode != 200) {
        result.errorMessage = "Schedule fetch failed: " + String(fetchCode);
        client->end();
        return result;
    }

    String fetchResponse = client->getString();
    client->end();

    Serial.print("[TEST]   Fetch response: ");
    Serial.println(fetchResponse);

    // Parse response to get jobId
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, fetchResponse);

    if (error) {
        Serial.print("[TEST]   JSON parse error: ");
        Serial.println(error.c_str());
        result.errorMessage = "Failed to parse schedule response";
        return result;
    }

    String jobId = doc["jobId"] | "";
    if (jobId.isEmpty()) {
        result.errorMessage = "No job ID in response";
        return result;
    }

    Serial.print("[TEST]   Fetch scheduled, job: ");
    Serial.print(jobId);
    Serial.println(", waiting for completion...");

    // Step 2: Wait for fetch to complete by polling status
    int maxPolls = 60; // 30 seconds max (500ms * 60)
    bool fetchComplete = false;
    String endpointHash = "";
    int actualStatusCode = -1;

    for (int i = 0; i < maxPolls; i++) {
        delay(500);

        String statusUrl = fetcherUrl + "/api/status/" + jobId;
        client->begin(*HTTPTestManager::getSharedClient(), statusUrl);
        int statusCode = client->GET();

        if (statusCode == 200) {
            String statusResponse = client->getString();
            client->end();

            JsonDocument statusDoc;
            if (!deserializeJson(statusDoc, statusResponse)) {
                String status = statusDoc["status"] | "";

                if (status == "completed") {
                    fetchComplete = true;
                    endpointHash = statusDoc["endpointHash"] | "";
                    actualStatusCode = statusDoc["statusCode"] | -1;
                    Serial.println("[TEST]   Fetch completed");
                    break;
                } else if (status == "failed") {
                    String errorMsg = statusDoc["errorMessage"] | "Unknown error";
                    result.errorMessage = "Fetch failed: " + errorMsg;
                    client->end();
                    return result;
                }
                // else status == "pending", continue polling
            }
        }
        client->end();
    }

    if (!fetchComplete) {
        result.errorMessage = "Fetch timeout waiting for completion";
        return result;
    }

    // Step 3: Verify status code
    if (actualStatusCode != expectedStatusCode) {
        result.errorMessage = "Expected status " + String(expectedStatusCode) +
                            ", got " + String(actualStatusCode);
        return result;
    }

    Serial.print("[TEST]   Endpoint cached, hash: ");
    Serial.println(endpointHash);

    // Step 4: Stream and search
    bool searchSuccess = streamAndSearch(fetcherUrl, endpointHash, searchTerms,
                                        searchTermCount, result.chunksProcessed, result.totalBytes);

    if (searchSuccess) {
        result.success = true;
        Serial.print("[TEST]   Test passed! Found all search terms in ");
        Serial.print(result.chunksProcessed);
        Serial.print(" chunks (");
        Serial.print(result.totalBytes);
        Serial.println(" bytes)");
    } else {
        result.errorMessage = "Not all search terms found";
    }

    return result;
}

bool CachedEndpointTester::streamAndSearch(
    const String& fetcherUrl,
    const String& endpointHash,
    const String searchTerms[],
    int searchTermCount,
    int& chunksProcessed,
    long& totalBytes)
{
    // Initialize search tracking
    bool found[searchTermCount];
    for (int i = 0; i < searchTermCount; i++) {
        found[i] = false;
    }

    const int CHUNK_SIZE = CACHED_ENDPOINT_TESTER_CHUNK_SIZE;
    int offset = 0;
    chunksProcessed = 0;
    totalBytes = 0;

    HTTPClient* client = HTTPTestManager::getClient();
    // Reuse a single JSON document to reduce heap churn/fragmentation
    // Sized for CHUNK_SIZE + JSON overhead
    JsonDocument doc;

    // Rolling overlap to catch cross-chunk matches without buffering entire response
    String overlap;
    const int OVERLAP_BYTES = CACHED_ENDPOINT_TESTER_OVERLAP_BYTES;

    while (true) {
        String streamUrl = fetcherUrl + "/api/stream/" + endpointHash +
                          "?offset=" + String(offset) +
                          "&chunkSize=" + String(CHUNK_SIZE);

        client->begin(*HTTPTestManager::getSharedClient(), streamUrl);
        int streamCode = client->GET();

        if (streamCode != 200) {
            client->end();
            return false;
        }

        // Read small JSON payload for this chunk and parse from String (safe size)
        String streamResponse = client->getString();
        client->end();
        doc.clear();
        DeserializationError error = deserializeJson(doc, streamResponse);

        if (error) {
            return false;
        }

        String chunk = doc["chunk"] | "";
        bool isLastChunk = doc["isLastChunk"] | false;
        // Prepare search buffer with overlap to handle cross-boundary matches
        String searchBuf;
        if (overlap.length() > 0) {
            searchBuf.reserve(overlap.length() + chunk.length());
            searchBuf = overlap;
            searchBuf += chunk;
        } else {
            searchBuf = chunk;
        }

        // Search for terms in combined buffer
        for (int i = 0; i < searchTermCount; i++) {
            if (!found[i] && searchBuf.indexOf(searchTerms[i]) >= 0) {
                found[i] = true;
                Serial.print("[TEST]     Found: ");
                Serial.println(searchTerms[i]);
            }
        }

        chunksProcessed++;
        totalBytes += chunk.length();
        offset += CHUNK_SIZE;

        // Update overlap with tail of current chunk
        if (chunk.length() > 0) {
            if ((int)chunk.length() > OVERLAP_BYTES) {
                overlap = chunk.substring(chunk.length() - OVERLAP_BYTES);
            } else {
                overlap = chunk;
            }
        } else {
            overlap = "";
        }

        // Check if all terms found
        bool allFound = true;
        for (int i = 0; i < searchTermCount; i++) {
            if (!found[i]) {
                allFound = false;
                break;
            }
        }
        if (allFound) {
            return true;
        }

        // Check if last chunk
        if (isLastChunk) {
            // Final check
            for (int i = 0; i < searchTermCount; i++) {
                if (!found[i]) {
                    Serial.print("[TEST]     Missing: ");
                    Serial.println(searchTerms[i]);
                }
            }
            return allFound;
        }

        // Safety limit
        if (offset > 100000) {
            return false;
        }

        // Yield to allow background tasks and reduce tight-loop pressure
        delay(1);
    }

    return false;
}
