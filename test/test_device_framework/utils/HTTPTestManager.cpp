#include "HTTPTestManager.h"
#include "Utils/TimeUtils.h"
#include <DeviceFrameworkPlatform.h>  // Platform abstraction (includes WiFi headers)

WiFiClient* HTTPTestManager::sharedClient = nullptr;
HTTPClient* HTTPTestManager::sharedHttpClient = nullptr;
bool HTTPTestManager::initialized = false;
unsigned long HTTPTestManager::lastUsedTime = 0;

bool HTTPTestManager::initialize() {
    if (initialized) {
        return true;
    }

    Serial.println("[TEST]   [HTTPTestManager] Initializing shared HTTP client pool...");

    // Create shared WiFi client
    sharedClient = new WiFiClient();
    if (!sharedClient) {
        Serial.println("[TEST]   [HTTPTestManager] Failed to create WiFiClient");
        return false;
    }

    // Create shared HTTP client
    sharedHttpClient = new HTTPClient();
    if (!sharedHttpClient) {
        Serial.println("[TEST]   [HTTPTestManager] Failed to create HTTPClient");
        delete sharedClient;
        sharedClient = nullptr;
        return false;
    }

    // Configure HTTP client with reasonable defaults
    sharedHttpClient->setTimeout(10000); // 10 second timeout

    initialized = true;
    lastUsedTime = millis();

    Serial.println("[TEST]   [HTTPTestManager] Shared HTTP client pool initialized");
    return true;
}

void HTTPTestManager::cleanup() {
    if (!initialized) {
        return;
    }

    Serial.println("[TEST]   [HTTPTestManager] Cleaning up shared HTTP client pool...");

    if (sharedHttpClient) {
        sharedHttpClient->end();
        delete sharedHttpClient;
        sharedHttpClient = nullptr;
    }

    if (sharedClient) {
        delete sharedClient;
        sharedClient = nullptr;
    }

    initialized = false;
    Serial.println("[TEST]   [HTTPTestManager] Shared HTTP client pool cleaned up");
}

HTTPClient* HTTPTestManager::getClient() {
    if (!initialized) {
        if (!initialize()) {
            return nullptr;
        }
    }

    // Check if we need to reconnect due to timeout
    if (!isConnectionValid()) {
        Serial.println("[TEST]   [HTTPTestManager] Connection expired, reconnecting...");
        reconnect();
    }

    lastUsedTime = millis();
    return sharedHttpClient;
}

WiFiClient* HTTPTestManager::getSharedClient() {
    if (!initialized) {
        if (!initialize()) {
            return nullptr;
        }
    }

    return sharedClient;
}

bool HTTPTestManager::isConnectionValid() {
    if (!initialized || !sharedClient || !sharedHttpClient) {
        return false;
    }

    // Check if connection has been idle too long
    if (TimeUtils::hasTimeElapsed(millis(), lastUsedTime, CONNECTION_TIMEOUT)) {
        return false;
    }

    // Check if WiFi is still connected
    if (!WiFi.isConnected()) {
        return false;
    }

    return true;
}

void HTTPTestManager::reconnect() {
    if (!initialized) {
        return;
    }

    Serial.println("[TEST]   [HTTPTestManager] Reconnecting HTTP client...");

    // End current connection
    if (sharedHttpClient) {
        sharedHttpClient->end();
    }

    // Reset the WiFi client
    if (sharedClient) {
        delete sharedClient;
        sharedClient = new WiFiClient();
    }

    lastUsedTime = millis();
    Serial.println("[TEST]   [HTTPTestManager] HTTP client reconnected");
}

void HTTPTestManager::printStats() {
    if (!initialized) {
        Serial.println("[TEST]   [HTTPTestManager] Not initialized");
        return;
    }

    unsigned long idleTime = TimeUtils::safeTimeDifference(millis(), lastUsedTime);
    Serial.printf("[TEST]   [HTTPTestManager] Stats - Idle: %lu ms, Valid: %s\n",
                  idleTime, isConnectionValid() ? "Yes" : "No");
}
