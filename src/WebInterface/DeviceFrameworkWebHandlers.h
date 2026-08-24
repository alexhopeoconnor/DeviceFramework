#ifndef DEVICEFRAMEWORK_WEB_HANDLERS_H
#define DEVICEFRAMEWORK_WEB_HANDLERS_H

#ifdef ENABLE_WEB_INTERFACE
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class DeviceFrameworkWebHandlers {
public:
    // HTTP request handlers
    static void handleWebRoot(AsyncWebServerRequest *request);
    static void handleWebNotFound(AsyncWebServerRequest *request);
    static void handleAPIStatus(AsyncWebServerRequest *request);
    static void handleAPIControl(AsyncWebServerRequest *request);

    // Requires HTTP Basic authentication only when a device password is set.
    static bool isAuthenticated(AsyncWebServerRequest *request);

    // Streaming response
    static void sendStreamingResponse(AsyncWebServerRequest *request, const char* baseTemplate);

    // JSON size estimation management (for sketch use)
    static void recalculateJSONSizeEstimation();
    static size_t getEstimatedJSONSize();
    static bool isJSONSizeEstimationInitialized();
};

#endif // ENABLE_WEB_INTERFACE
#endif // DEVICEFRAMEWORK_WEB_HANDLERS_H
