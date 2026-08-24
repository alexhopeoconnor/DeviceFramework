#ifndef DEVICEFRAMEWORK_WIFI_MANAGER_LOG_SINK_H
#define DEVICEFRAMEWORK_WIFI_MANAGER_LOG_SINK_H

#include <WiFiManagerLogSink.h>

/**
 * Routes WiFiManager log lines into DeviceFramework LOG_* macros.
 */
class DeviceFrameworkWiFiManagerLogSink : public WiFiManagerLogSink {
public:
    void log(const WiFiManagerLogMessage& msg) override;
};

#endif
