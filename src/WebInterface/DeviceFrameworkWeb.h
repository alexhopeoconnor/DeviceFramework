#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#ifdef ENABLE_WEB_INTERFACE
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <TemplateEngine.h>
#include "DeviceFrameworkWebHandlers.h"
#include "DeviceFrameworkWebResourceLimits.h"
#include "DeviceFrameworkTemplatePlaceholders.h"

class DeviceFrameworkWeb {
public:
    // Main interface methods
    static void setup();
    static void loop();
    // Reconciles the HTTP service without disrupting a live async listener.
    static void restart();
    static void shutdown();
    static bool isEnabled();
    static bool isInConfigMode();

    // Resource limits are configured by the consuming sketch before setup().
    // They are deliberately runtime-only and never persisted with device
    // configuration or local profiles.
    static DeviceFrameworkWebResourceLimits defaultResourceLimits();
    static bool setResourceLimits(const DeviceFrameworkWebResourceLimits& limits);
    static const DeviceFrameworkWebResourceLimits& getResourceLimits();
    static DeviceFrameworkWebResourceStats getResourceStats();

    // Access to the placeholder registry (for handlers)
    static PlaceholderRegistry* getRegistry();

private:
    static AsyncWebServer* webServer;
    static bool webInterfaceEnabled;
    static bool resourceLimitsLocked;
    static DeviceFrameworkWebResourceLimits resourceLimits;

    // Internal cleanup method
    static void cleanup();
    static bool validateResourceLimits(const DeviceFrameworkWebResourceLimits& limits);
};

#endif // ENABLE_WEB_INTERFACE
#endif // WEB_INTERFACE_H
