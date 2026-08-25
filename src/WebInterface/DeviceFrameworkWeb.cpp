#ifdef ENABLE_WEB_INTERFACE
#include "DeviceFrameworkWeb.h"
#include "DeviceFrameworkWebHandlers.h"
#include "DeviceFrameworkWebSerial.h"
#include "DeviceFrameworkTemplatePlaceholders.h"
#include "DeviceFrameworkDeviceStatus.h"
#include "WebInterfaceTemplateEngineLogger.h"
#include "../WiFi/DeviceFrameworkWiFi.h"
#include "../Utils/TimeUtils.h"

AsyncWebServer* DeviceFrameworkWeb::webServer = nullptr;
bool DeviceFrameworkWeb::webInterfaceEnabled = false;

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

    webServer = new AsyncWebServer(80);

    // Initialize JSON size estimation with real data before setting up routes
    DeviceStatusManager::initializeHardwareInfo();
    DeviceStatusManager::updateRuntimeInfo();
    DeviceStatusManager::initializeJSONSizeEstimation();

    // Initialize runtime tracking after all systems are up
    DeviceStatusManager::initializeRuntimeTracking();

    // Set up routes
    webServer->on("/", HTTP_GET, DeviceFrameworkWebHandlers::handleWebRoot);
    // Browsers request this immediately after "/"; avoid streaming a full 404 HTML page (heap + TCP load).
    webServer->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(204);
    });
    webServer->on("/api/status", HTTP_GET, DeviceFrameworkWebHandlers::handleAPIStatus);
    webServer->on("/api/control", HTTP_POST, DeviceFrameworkWebHandlers::handleAPIControl);
    webServer->onNotFound(DeviceFrameworkWebHandlers::handleWebNotFound);

    // Initialize WebSerial transport (read-only serial output)
    DeviceFrameworkWebSerial::begin(webServer, "/webserial");
    if (getConfigDevicePassword()[0] != '\0') {
        DeviceFrameworkWebSerial::setAuthentication("admin", getConfigDevicePassword());
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

    // Reset state
    webInterfaceEnabled = false;
}

void DeviceFrameworkWeb::restart() {
    LOG_DEBUGLN(F("Restarting web interface..."));

    // Destroying an active AsyncWebServer can leave ESP8266 TCP callbacks
    // pointing at freed handlers. Keep routes and WebSerial ownership stable;
    // restart only the listening socket. Dynamic route authentication reads the
    // current shared device password for each request.
    if (DeviceFrameworkWiFi::isInConfigMode()) {
        shutdown();
        return;
    }
    if (!webServer) {
        setup();
        return;
    }

    webServer->end();
    // Give ESPAsyncTCP one scheduler turn to release the previous listener.
    delay(50);
    webServer->begin();
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