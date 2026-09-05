#ifdef ENABLE_WEB_INTERFACE
#include "DeviceFrameworkWeb.h"
#include "DeviceFrameworkWebHandlers.h"
#include "DeviceFrameworkWebAdmissionControl.h"
#include "DeviceFrameworkWebSerial.h"
#include "DeviceFrameworkTemplatePlaceholders.h"
#include "DeviceFrameworkDeviceStatus.h"
#include "WebInterfaceTemplateEngineLogger.h"
#include "../WiFi/DeviceFrameworkWiFi.h"
#include "../DeviceFramework.h"
#include "../Utils/TimeUtils.h"
#include <DeviceFrameworkPlatform.h>

AsyncWebServer* DeviceFrameworkWeb::webServer = nullptr;
bool DeviceFrameworkWeb::webInterfaceEnabled = false;
bool DeviceFrameworkWeb::resourceLimitsLocked = false;
DeviceFrameworkWebResourceLimits DeviceFrameworkWeb::resourceLimits =
    DeviceFrameworkWeb::defaultResourceLimits();

DeviceFrameworkWebResourceLimits DeviceFrameworkWeb::defaultResourceLimits() {
#if defined(DF_PLATFORM_ESP8266)
    return {
        1,
        2,
        {
            12UL * 1024UL,
            8UL * 1024UL,
            8UL * 1024UL,
            6UL * 1024UL,
        },
        WebSerialAdmissionPolicy::PreserveExisting,
    };
#else
    return {
        6,
        12,
        {
            64UL * 1024UL,
            32UL * 1024UL,
            32UL * 1024UL,
            16UL * 1024UL,
        },
        WebSerialAdmissionPolicy::PreserveExisting,
    };
#endif
}

bool DeviceFrameworkWeb::validateResourceLimits(
    const DeviceFrameworkWebResourceLimits& limits) {
    if (limits.maxConcurrentStreamResponses == 0 ||
        limits.maxConcurrentStreamResponses > DeviceFrameworkWebAdmissionControl::kMaximumStreamPermits ||
        limits.maxWebSerialClients == 0 ||
        limits.maxWebSerialClients > DeviceFrameworkWebAdmissionControl::kMaximumTrackedWebSerialClients) {
        return false;
    }

    return limits.memory.shedWebSerialBelowFreeHeapBytes <=
               limits.memory.rejectNewBelowFreeHeapBytes &&
           limits.memory.shedWebSerialBelowLargestBlockBytes <=
               limits.memory.rejectNewBelowLargestBlockBytes;
}

bool DeviceFrameworkWeb::setResourceLimits(
    const DeviceFrameworkWebResourceLimits& limits) {
    if (resourceLimitsLocked || !validateResourceLimits(limits)) {
        return false;
    }
    resourceLimits = limits;
    return true;
}

const DeviceFrameworkWebResourceLimits& DeviceFrameworkWeb::getResourceLimits() {
    return resourceLimits;
}

DeviceFrameworkWebResourceStats DeviceFrameworkWeb::getResourceStats() {
    return DeviceFrameworkWebAdmissionControl::stats();
}

void DeviceFrameworkWeb::setup() {
    if (webServer) {
        cleanup();
    }

    if (DeviceFrameworkWiFi::isInConfigMode()) {
        LOG_INFOLN(F("Web interface disabled - WiFiManager in config mode"));
        return;
    }

    // Setup template engine logging integration first (needed for template engine logging)
    setupWebInterfaceTemplateEngineLogging();

    // Setup template placeholders (creates registry and registers all placeholders)
    DeviceFrameworkTemplatePlaceholders::setup();

    DeviceFrameworkWebAdmissionControl::begin(resourceLimits);
    resourceLimitsLocked = true;

    webServer = new AsyncWebServer(80);

    // Initialize runtime tracking before the first status snapshot.
    DeviceStatusManager::initializeHardwareInfo();
    DeviceStatusManager::initializeRuntimeTracking();
    DeviceStatusManager::updateRuntimeInfo();
    DeviceStatusManager::initializeJSONSizeEstimation();

    // Set up routes
    webServer->on("/", HTTP_GET, DeviceFrameworkWebHandlers::handleWebRoot);
    // Keep only handlers that require AsyncWebServer-specific behavior.
    // Simple assets and fixed GET/POST endpoints share the fallback dispatcher
    // below, avoiding one permanent heap allocation per endpoint on ESP8266.
    webServer->on("/api/control", HTTP_POST, DeviceFrameworkWebHandlers::handleAPIControl,
                  nullptr, DeviceFrameworkWebHandlers::handleAPIControlBody);
    webServer->onNotFound(DeviceFrameworkWebHandlers::handleWebNotFound);

    // Initialize WebSerial transport (read-only serial output)
    DeviceFrameworkWebSerial::begin(webServer, "/webserial");
    if (DeviceFramework::getDevicePassword()[0] != '\0') {
        DeviceFrameworkWebSerial::setAuthentication("admin", DeviceFramework::getDevicePassword());
    }

    webServer->begin();
    webInterfaceEnabled = true;

    LOG_INFOLN(F("Web interface started on port 80"));
}

void DeviceFrameworkWeb::cleanup() {
    // Clean up WebSerial first - close all connections
    DeviceFrameworkWebSerial::end();

    shutdownWebInterfaceTemplateEngineLogging();

    // Then stop and delete the server
    if (webServer) {
        webServer->end();
        delete webServer;
        webServer = nullptr;
    }

    // Cleanup template placeholders (destroys registry)
    DeviceFrameworkTemplatePlaceholders::cleanup();

    DeviceFrameworkWebAdmissionControl::end();

    // Reset state
    webInterfaceEnabled = false;
}

void DeviceFrameworkWeb::restart() {
    LOG_DEBUGLN(F("Restarting web interface..."));

    if (DeviceFrameworkWiFi::isInConfigMode()) {
        shutdown();
        return;
    }

    if (!webServer) {
        setup();
        return;
    }

    // On ESP8266, AsyncWebServer::end() followed by begin() on the same
    // instance can leave port 80 unreachable. Routes and HTTP authentication
    // already read their live state, so an existing listener needs no rebind.
    webInterfaceEnabled = true;
}

void DeviceFrameworkWeb::shutdown() {
    LOG_DEBUGLN(F("Shutting down web interface..."));

    // Clean up resources
    cleanup();

    LOG_INFOLN(F("Web interface shut down"));
}

bool DeviceFrameworkWeb::isEnabled() {
    return webInterfaceEnabled;
}

bool DeviceFrameworkWeb::isInConfigMode() {
    return DeviceFrameworkWiFi::isInConfigMode();
}

PlaceholderRegistry* DeviceFrameworkWeb::getRegistry() {
    return DeviceFrameworkTemplatePlaceholders::getRegistry();
}

void DeviceFrameworkWeb::loop() {
    if (!webInterfaceEnabled) {
        return;
    }

    unsigned long currentMillis = millis();

    // Update runtime status periodically (every 2 seconds, but only if needed)
    static unsigned long lastStatusUpdate = 0;
    if (TimeUtils::hasTimeElapsed(currentMillis, lastStatusUpdate, 2000)) {
        // Only update if we have pending web requests or if status is stale
        if (DeviceStatusManager::needsUpdate()) {
            DeviceStatusManager::updateRuntimeInfo();
        }
        lastStatusUpdate = currentMillis;
    }

    // Handle WebSerial transport maintenance (buffering, flushing, etc.)
    DeviceFrameworkWebSerial::loop();
}

#endif // ENABLE_WEB_INTERFACE