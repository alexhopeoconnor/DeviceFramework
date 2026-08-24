#ifndef WEB_INTERFACE_TEMPLATE_ENGINE_LOGGER_H
#define WEB_INTERFACE_TEMPLATE_ENGINE_LOGGER_H

#include <DeviceFrameworkTemplateEngineDebug.h>
#include "DeviceFrameworkDebug.h"

/**
 * Web Interface Template Engine Logger
 *
 * Bridges DeviceFramework's logging system to the template engine.
 * This allows the template engine to use DeviceFramework's sophisticated
 * logging system including log levels, WebSocket output, and memory logging.
 */
class WebInterfaceTemplateEngineLogger : public DeviceFrameworkTemplateEngineLogger {
public:
    void error(const String& msg) override {
        LOG_ERRORLN(msg);
    }

    void warn(const String& msg) override {
        LOG_WARNLN(msg);
    }

    void info(const String& msg) override {
        LOG_INFOLN(msg);
    }

    void debug(const String& msg) override {
        LOG_DEBUGLN(msg);
    }
};

/**
 * Register the DeviceFramework DFTE logger (owner-tagged) so template engine
 * output uses DeviceFramework's LOG_* macros.
 */
void setupWebInterfaceTemplateEngineLogging();

/** Disable logging only if it was registered by setupWebInterfaceTemplateEngineLogging (same owner). */
void shutdownWebInterfaceTemplateEngineLogging();

#endif // WEB_INTERFACE_TEMPLATE_ENGINE_LOGGER_H
