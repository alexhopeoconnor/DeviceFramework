#include "DeviceFrameworkArduinoHALogSink.h"
#include "DeviceFrameworkDebug.h"

void DeviceFrameworkArduinoHALogSink::log(const ArduinoHALogMessage& msg) {
    const String line =
        String(F("[aha.")) + (msg.subsystem ? msg.subsystem : "aha") + F("] ") + msg.text;

    switch (msg.level) {
        case ArduinoHALogLevel::Error:
            LOG_ERRORLN(line);
            break;
        case ArduinoHALogLevel::Warn:
            LOG_WARNLN(line);
            break;
        case ArduinoHALogLevel::Info:
            LOG_INFOLN(line);
            break;
        case ArduinoHALogLevel::Debug:
        case ArduinoHALogLevel::Trace:
            LOG_DEBUGLN(line);
            break;
        default:
            LOG_DEBUGLN(line);
            break;
    }
}
