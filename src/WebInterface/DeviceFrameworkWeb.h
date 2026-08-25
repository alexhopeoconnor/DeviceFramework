#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#ifdef ENABLE_WEB_INTERFACE
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <TemplateEngine.h>
#include "DeviceFrameworkWebHandlers.h"
#include "DeviceFrameworkTemplatePlaceholders.h"

class DeviceFrameworkWeb {
public:
    // Main interface methods
    static void setup();
    static void loop();
    // Safely rebinds the HTTP listener without destroying active async handlers.
    static void restart();
    static void shutdown();
    static bool isEnabled();
    static bool isInConfigMode();

    // Access to the placeholder registry (for handlers)
    static PlaceholderRegistry* getRegistry();

private:
    static AsyncWebServer* webServer;
    static bool webInterfaceEnabled;

    // Internal cleanup method
    static void cleanup();
};

#endif // ENABLE_WEB_INTERFACE
#endif // WEB_INTERFACE_H
