#ifdef ENABLE_WEB_INTERFACE
// Include DeviceFrameworkConfig.h first to ensure config defaults are available
// when template headers are processed via TemplateEngine.h
#include "../Configuration/DeviceFrameworkConfig.h"
#include "DeviceFrameworkWebHandlers.h"
#include <memory>
#include "DeviceFrameworkDeviceStatus.h"
#include "../Utils/TimeUtils.h"
#include "DeviceFrameworkWeb.h"
#include <TemplateEngine.h>
#include <TemplateEngineAsyncWeb.h>
#include "templates/WebInterfaceHTML.h"
#include "../DeviceFrameworkDebug.h"
#include "../WiFi/DeviceFrameworkWiFi.h"
#include "../MQTT/DeviceFrameworkMQTT.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../Configuration/DeviceFrameworkConfig.h"

namespace {

struct StatusStreamResponseState {
    DeviceStatusManager::JSONStreamState stream;
    bool started;

    StatusStreamResponseState() : stream(), started(false) {}
};

} // namespace

bool DeviceFrameworkWebHandlers::isAuthenticated(AsyncWebServerRequest *request) {
    const char* password = getConfigDevicePassword();
    if (password == nullptr || password[0] == '\0') {
        return true;
    }

    if (request->authenticate("admin", password)) {
        return true;
    }

    request->requestAuthentication();
    return false;
}

void DeviceFrameworkWebHandlers::handleWebRoot(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;
    sendStreamingResponse(request, base_template);
}

void DeviceFrameworkWebHandlers::handleWebNotFound(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;
    sendStreamingResponse(request, error404_template);
}

void DeviceFrameworkWebHandlers::handleAPIStatus(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;
    // Update status before building response
    DeviceStatusManager::updateRuntimeInfo();
    auto responseState = std::make_shared<StatusStreamResponseState>();
    DeviceStatusManager::resetJSONStreamState(responseState->stream);

    // Stream the status JSON directly into the response buffer so we don't
    // materialize the entire document in heap memory first.
    AsyncWebServerResponse *response = TemplateEngineAsyncWeb::beginSafeChunkedResponse(
        request,
        "application/json",
        responseState,
        [](StatusStreamResponseState& state, uint8_t *buffer, size_t maxLen, size_t /*index*/) -> size_t {
            if (!state.started) {
                DeviceStatusManager::resetJSONStreamState(state.stream);
                state.started = true;
            }

            return DeviceStatusManager::renderJSONChunk(
                state.stream, buffer, maxLen, DeviceStatusManager::getStatus());
        },
        [](const StatusStreamResponseState& state) -> bool {
            return state.stream.complete;
        });

    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");

    request->send(response);
}

void DeviceFrameworkWebHandlers::handleAPIControl(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;
    if (request->hasParam("body")) {
        String body = request->getParam("body")->value();

        if (body.indexOf("\"action\":\"restart\"") >= 0) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Restart command received\"}");
            delay(1000);
            ESP.restart();
        } else if (body.indexOf("\"action\":\"reset\"") >= 0) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Reset command received\"}");
        } else if (body.indexOf("\"action\":\"clear_eeprom\"") >= 0) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Clear EEPROM command received\"}");
        } else if (body.indexOf("\"action\":\"config_mode\"") >= 0) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Config mode command received\"}");
        } else if (body.indexOf("\"action\":\"disconnect_wifi\"") >= 0) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Disconnect WiFi command received\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Unknown action\"}");
        }
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No action specified\"}");
    }
}

void DeviceFrameworkWebHandlers::sendStreamingResponse(AsyncWebServerRequest *request, const char* baseTemplate) {
    LOG_INFOLN(String(F("Starting streaming response: ")) + request->url());

    // Get registry from web interface (may be nullptr if disabled)
    PlaceholderRegistry* registry = DeviceFrameworkWeb::getRegistry();
    if (!registry) {
        request->send(500, "text/plain", "Web interface not initialized");
        return;
    }

    auto ctx = std::shared_ptr<TemplateContext>(new TemplateContext());
    ctx->setRegistry(registry);
    TemplateRenderer::initializeContext(*ctx, baseTemplate);

    AsyncWebServerResponse *response = TemplateEngineAsyncWeb::beginSafeTemplateResponse(
        request, "text/html", ctx, 128);

    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");

    request->send(response);
}

// Public API methods for sketch integration
void DeviceFrameworkWebHandlers::recalculateJSONSizeEstimation() {
    DeviceStatusManager::recalculateJSONSizeEstimation();
}

size_t DeviceFrameworkWebHandlers::getEstimatedJSONSize() {
    return DeviceStatusManager::getEstimatedJSONSize();
}

bool DeviceFrameworkWebHandlers::isJSONSizeEstimationInitialized() {
    return DeviceStatusManager::isJSONSizeEstimationInitialized();
}

#endif // ENABLE_WEB_INTERFACE
