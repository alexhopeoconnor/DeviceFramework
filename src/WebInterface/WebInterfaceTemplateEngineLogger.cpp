#include "WebInterfaceTemplateEngineLogger.h"

namespace {

WebInterfaceTemplateEngineLogger g_webInterfaceTemplateEngineLogger;

// Stable owner tag for deviceFrameworkTemplateEngineDisableLoggingForOwner (WiFiManager uses `this`).
const void* const kWebInterfaceTemplateEngineLoggerOwner =
    static_cast<const void*>(&g_webInterfaceTemplateEngineLogger);

} // namespace

void setupWebInterfaceTemplateEngineLogging() {
    deviceFrameworkTemplateEngineEnableLogging(&g_webInterfaceTemplateEngineLogger,
                                               kWebInterfaceTemplateEngineLoggerOwner);
    LOG_INFOLN(F("Web Interface Template Engine logging enabled"));
}

void shutdownWebInterfaceTemplateEngineLogging() {
    deviceFrameworkTemplateEngineDisableLoggingForOwner(kWebInterfaceTemplateEngineLoggerOwner);
}
