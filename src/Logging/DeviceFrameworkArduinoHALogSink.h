#ifndef DEVICEFRAMEWORK_ARDUINOHA_LOG_SINK_H
#define DEVICEFRAMEWORK_ARDUINOHA_LOG_SINK_H

#include <ArduinoHALog.h>

/**
 * Routes structured ArduinoHA logs into DeviceFramework's leveled Serial / WebSerial output.
 */
class DeviceFrameworkArduinoHALogSink : public ArduinoHALogSink {
public:
    void log(const ArduinoHALogMessage& msg) override;
};

#endif
