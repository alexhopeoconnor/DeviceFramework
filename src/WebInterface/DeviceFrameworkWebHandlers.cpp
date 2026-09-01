#ifdef ENABLE_WEB_INTERFACE
// Include DeviceFrameworkConfig.h first to ensure config defaults are available
// when template headers are processed via TemplateEngine.h
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../DeviceFramework.h"
#include "DeviceFrameworkWebHandlers.h"
#include <memory>
#include <new>
#include "DeviceFrameworkDeviceStatus.h"
#include "../Utils/TimeUtils.h"
#include "DeviceFrameworkWeb.h"
#include <TemplateEngine.h>
#include <TemplateEngineAsyncWeb.h>
#include "templates/WebInterfaceHTML.h"
#include "templates/WebInterfaceCSS.h"
#include "templates/WebInterfaceJS.h"
#include "../DeviceFrameworkDebug.h"
#include "../WiFi/DeviceFrameworkWiFi.h"
#include "../MQTT/DeviceFrameworkMQTT.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../UI/DeviceFrameworkUI.h"

namespace {

struct StatusStreamResponseState {
    DeviceStatusManager::JSONStreamState stream;
    bool started;
    StatusStreamResponseState() : stream(), started(false) {}
};

struct Base64LogoResponseState {
    const char* data;
    size_t length;
    size_t sourceOffset;
    bool progmem;
    uint8_t pending[3];
    uint8_t pendingOffset;
    uint8_t pendingLength;
    bool inputFinished;
    bool complete;

    Base64LogoResponseState(const char* source, size_t sourceLength, bool sourceInProgmem)
        : data(source), length(sourceLength), sourceOffset(0), progmem(sourceInProgmem),
          pending{0, 0, 0}, pendingOffset(0), pendingLength(0),
          inputFinished(false), complete(false) {}
};

char readBase64LogoByte(const Base64LogoResponseState& state, size_t offset) {
    return state.progmem ? static_cast<char>(pgm_read_byte(state.data + offset))
                         : state.data[offset];
}

int8_t decodeBase64Character(char value) {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

bool fillDecodedLogoBytes(Base64LogoResponseState& state) {
    int8_t quartet[4] = {0, 0, 0, 0};
    uint8_t count = 0;
    while (count < 4 && state.sourceOffset < state.length) {
        const char value = readBase64LogoByte(state, state.sourceOffset++);
        if (value == ' ' || value == '\n' || value == '\r' || value == '\t') {
            continue;
        }
        if (value == '=') {
            quartet[count++] = -2;
            continue;
        }
        const int8_t decoded = decodeBase64Character(value);
        if (decoded < 0) {
            continue;
        }
        quartet[count++] = decoded;
    }

    if (count != 4 || quartet[0] < 0 || quartet[1] < 0 ||
        (quartet[2] == -2 && quartet[3] != -2)) {
        state.inputFinished = true;
        return false;
    }

    state.pending[0] = static_cast<uint8_t>((quartet[0] << 2) | (quartet[1] >> 4));
    state.pendingLength = 1;
    if (quartet[2] != -2) {
        state.pending[1] = static_cast<uint8_t>((quartet[1] << 4) | (quartet[2] >> 2));
        state.pendingLength = 2;
        if (quartet[3] != -2) {
            state.pending[2] = static_cast<uint8_t>((quartet[2] << 6) | quartet[3]);
            state.pendingLength = 3;
        }
    }
    state.pendingOffset = 0;
    state.inputFinished = state.sourceOffset >= state.length || quartet[2] == -2 || quartet[3] == -2;
    return true;
}

size_t renderDecodedLogoChunk(Base64LogoResponseState& state, uint8_t* buffer, size_t maxLen) {
    size_t written = 0;
    while (written < maxLen) {
        if (state.pendingOffset >= state.pendingLength) {
            state.pendingOffset = 0;
            state.pendingLength = 0;
            if (!fillDecodedLogoBytes(state)) {
                break;
            }
        }

        const size_t available = state.pendingLength - state.pendingOffset;
        const size_t toCopy = min(available, maxLen - written);
        memcpy(buffer + written, state.pending + state.pendingOffset, toCopy);
        written += toCopy;
        state.pendingOffset += toCopy;
    }

    state.complete = state.inputFinished && state.pendingOffset >= state.pendingLength;
    return written;
}

bool restartPending = false;
unsigned long restartAt = 0;

// DeviceFramework's built-in pages nest only a few templates. Keep their
// per-request renderer allocation deliberately lean on ESP8266 while DFTE
// retains its larger standalone defaults for callers that need them.
constexpr size_t kWebTemplateStackDepth = 6;
constexpr size_t kWebTemplateReadBufferSize = 128;

void scheduleRestart() {
    restartPending = true;
    restartAt = millis() + 500;
}

bool restartDue(unsigned long now) {
    return restartPending && static_cast<long>(now - restartAt) >= 0;
}

} // namespace

bool DeviceFrameworkWebHandlers::isAuthenticated(AsyncWebServerRequest *request) {
    const char* password = DeviceFramework::getDevicePassword();
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

namespace {

void sendStaticProgmemResponse(AsyncWebServerRequest* request, const char* contentType,
                               const char* content, size_t contentLength) {
    AsyncWebServerResponse* response = request->beginResponse(
        200, contentType, reinterpret_cast<const uint8_t*>(content), contentLength);
    // The asset URLs are stable within a running firmware. A short private
    // cache avoids repeatedly streaming large static assets while keeping an
    // OTA update visible without a long-lived stale browser cache.
    response->addHeader("Cache-Control", "private, max-age=300");
    request->send(response);
}

} // namespace

void DeviceFrameworkWebHandlers::handleWebStyles(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;
    sendStaticProgmemResponse(request, "text/css", css_styles, strlen_P(css_styles));
}

void DeviceFrameworkWebHandlers::handleWebScripts(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;
    sendStaticProgmemResponse(request, "application/javascript", js_scripts, strlen_P(js_scripts));
}

void DeviceFrameworkWebHandlers::handleWebLogo(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;

    const DeviceFrameworkWebLogo& logo = DeviceFrameworkUI::getWebLogo();
    if (logo.base64Data == nullptr) {
        request->send(204);
        return;
    }

    const size_t encodedLength = logo.progmem ? strlen_P(logo.base64Data) : strlen(logo.base64Data);
    if (encodedLength == 0) {
        request->send(204);
        return;
    }
    auto responseState = std::make_shared<Base64LogoResponseState>(
        logo.base64Data, encodedLength, logo.progmem);
    const char* contentType = (logo.mimeType && logo.mimeType[0]) ? logo.mimeType : "image/png";
    AsyncWebServerResponse* response = TemplateEngineAsyncWeb::beginSafeChunkedResponse(
        request, contentType, responseState,
        [](Base64LogoResponseState& state, uint8_t* buffer, size_t maxLen, size_t /*index*/) -> size_t {
            return renderDecodedLogoChunk(state, buffer, maxLen);
        },
        [](const Base64LogoResponseState& state) -> bool {
            return state.complete;
        });
    response->addHeader("Cache-Control", "private, max-age=300");
    request->send(response);
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
            scheduleRestart();
        } else if (body.indexOf("\"action\":\"reset\"") >= 0) {
            DeviceFramework::reset(DeviceFrameworkResetScope::ParametersOnly);
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration reset\"}");
            scheduleRestart();
        } else if (body.indexOf("\"action\":\"factory_reset\"") >= 0) {
            DeviceFramework::reset(DeviceFrameworkResetScope::Factory);
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Factory reset\"}");
            scheduleRestart();
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

void DeviceFrameworkWebHandlers::handleAPIDevicePassword(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return;
    if (!request->hasParam("new_password", true) || !request->hasParam("confirm_password", true)) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Password and confirmation are required\"}");
        return;
    }

    const String password = request->getParam("new_password", true)->value();
    const String confirmation = request->getParam("confirm_password", true)->value();
    if (password != confirmation) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Passwords do not match\"}");
        return;
    }
    if (password.length() == 0 &&
        (!request->hasParam("allow_empty", true) || request->getParam("allow_empty", true)->value() != "true")) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Explicit confirmation is required to remove the password\"}");
        return;
    }
    if (!DeviceFramework::setDevicePassword(password.c_str())) {
        request->send(422, "application/json", "{\"status\":\"error\",\"message\":\"Password must be empty or 8-31 characters, and storage must be writable\"}");
        return;
    }

    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Device password updated; restarting\"}");
    scheduleRestart();
}

void DeviceFrameworkWebHandlers::loop() {
    if (restartDue(millis())) {
        restartPending = false;
        ESP.restart();
    }
}


void DeviceFrameworkWebHandlers::sendStreamingResponse(AsyncWebServerRequest *request, const char* baseTemplate) {
    LOG_DEBUGLN(String(F("Starting streaming response: ")) + request->url());
    PlaceholderRegistry* registry = DeviceFrameworkWeb::getRegistry();
    // Get registry from web interface (may be nullptr if disabled)
    if (!registry) {
        request->send(500, "text/plain", "Web interface not initialized");
        return;
    }

    TemplateContext* rawContext = new (std::nothrow) TemplateContext(
        kWebTemplateStackDepth, kWebTemplateReadBufferSize);
    if (rawContext == nullptr || !rawContext->isReady()) {
        delete rawContext;
        request->send(503, "text/plain", "Web interface temporarily unavailable");
        return;
    }

    auto ctx = std::shared_ptr<TemplateContext>(rawContext);
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
