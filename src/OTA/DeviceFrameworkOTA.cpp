#include "DeviceFrameworkOTA.h"
#include "../WiFi/DeviceFrameworkWiFi.h"
#include "../DeviceFramework.h"
#include <DeviceFrameworkPlatform.h>

void DeviceFrameworkOTA::setup() {
    ArduinoOTA.onStart([]() {
        String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
        LOG_INFO_SP(F("OTA Update Started: "), true);
        LOG_INFOLN_SP(type, false);
    });

    ArduinoOTA.onEnd([]() {
        LOG_INFOLN(F("\nOTA Update Completed."));
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // Only show progress every 10% to reduce noise
        static unsigned int lastProgress = 0;
        unsigned int currentProgress = (progress * 100) / total;
        if (currentProgress >= lastProgress + 10 || currentProgress == 100) {
            LOG_DEBUG_SP(F("OTA Progress: "), true);
            LOG_DEBUG_SP(currentProgress, false);
            LOG_DEBUGLN_SP(F("%"), false);
            lastProgress = currentProgress;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        LOG_DEBUG_SP(F("OTA Error ["), true);
        LOG_DEBUG_SP(error, false);
        LOG_DEBUG_SP(F("]: "), false);
        switch (error) {
            case OTA_AUTH_ERROR: LOG_WARNLN(F("Auth Failed")); break;
            case OTA_BEGIN_ERROR: LOG_WARNLN(F("Begin Failed")); break;
            case OTA_CONNECT_ERROR: LOG_WARNLN(F("Connect Failed")); break;
            case OTA_RECEIVE_ERROR: LOG_WARNLN(F("Receive Failed")); break;
            case OTA_END_ERROR: LOG_WARNLN(F("End Failed")); break;
        }
    });

    // Set the OTA hostname and authentication password
    ArduinoOTA.setHostname(DeviceFramework::getSanitizedHostname());
    if (DeviceFramework::getDevicePassword()[0] != '\0') {
        ArduinoOTA.setPassword(DeviceFramework::getDevicePassword());
    }

    // On ESP8266 ArduinoOTA otherwise creates and updates the global mDNS
    // responder itself. DeviceFramework owns that responder so it can keep the
    // core's packet parser out of a fragmented heap; OTA remains reachable by
    // the device hostname or IP on its normal UDP port.
#ifdef DF_PLATFORM_ESP8266
    ArduinoOTA.begin(false);
#else
    ArduinoOTA.begin();
#endif
    LOG_INFO_SP(F("OTA Initialized with hostname: "), true);
    LOG_INFOLN_SP(String(DeviceFramework::getSanitizedHostname()), false);
}

void DeviceFrameworkOTA::loop() {
    // Handle OTA updates
    ArduinoOTA.handle();
}
