#include "DeviceFrameworkWiFiManagerLogSink.h"
#include <WiFiManager.h>
#include "DeviceFrameworkDebug.h"

void DeviceFrameworkWiFiManagerLogSink::log(const WiFiManagerLogMessage& wmMsg) {
    // Parameter must not be named `msg`: DeviceFramework LOG_* macros declare inner `String msg`.
    switch (wmMsg.level) {
        case WiFiManagerLogLevel::Error:
            LOG_ERRORLN(wmMsg.line);
            break;
        case WiFiManagerLogLevel::Warn:
            LOG_WARNLN(wmMsg.line);
            break;
        case WiFiManagerLogLevel::Info:
            LOG_INFOLN(wmMsg.line);
            break;
        case WiFiManagerLogLevel::Debug:
        case WiFiManagerLogLevel::Trace:
            LOG_DEBUGLN(wmMsg.line);
            break;
        case WiFiManagerLogLevel::Silent:
        default:
            break;
    }
}
