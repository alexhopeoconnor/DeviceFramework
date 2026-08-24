#ifndef HTTP_TEST_MANAGER_H
#define HTTP_TEST_MANAGER_H

#include <WiFiClient.h>
#include <DeviceFrameworkPlatform.h>  // Platform abstraction (includes HTTP client headers)
#include <Arduino.h>

class HTTPTestManager {
private:
    static WiFiClient* sharedClient;
    static HTTPClient* sharedHttpClient;
    static bool initialized;
    static unsigned long lastUsedTime;
    static const unsigned long CONNECTION_TIMEOUT = 30000; // 30 seconds

public:
    // Initialize the shared client pool
    static bool initialize();

    // Clean up resources
    static void cleanup();

    // Get a ready-to-use HTTP client
    static HTTPClient* getClient();

    // Get the shared WiFi client (for HTTPClient::begin)
    static WiFiClient* getSharedClient();

    // Release the client back to the pool (optional - auto-managed)
    static void releaseClient();

    // Check if connection is still valid
    static bool isConnectionValid();

    // Force reconnect if needed
    static void reconnect();

    // Get connection statistics
    static void printStats();
};

#endif
