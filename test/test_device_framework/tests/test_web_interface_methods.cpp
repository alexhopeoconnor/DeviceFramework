#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <DeviceFramework.h>
#include <WiFiClient.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <TemplateEngine.h>
#include <WebInterface/DeviceFrameworkTemplatePlaceholders.h>
#include <WebInterface/DeviceFrameworkDeviceStatus.h>
#include <Utils/PrintAdapters.h>
#include <WebInterface/templates/WebInterfaceHTML.h>

namespace {

// ESPAsyncWebServer can provide 1,064-byte response chunks on ESP8266. Keep
// this verification buffer in BSS: production receives that server-owned
// buffer, so allocating another one from the fixture heap would test an
// artificial failure mode and can invoke the ESP8266 core OOM exception.
constexpr size_t kAsyncWebChunkSize = 1064;
uint8_t asyncWebChunkBuffer[kAsyncWebChunkSize];

void assertTemplateCompletesIntoBuffer(const char* templateData, const char* description,
                                       uint8_t* buffer, size_t bufferSize) {
    TemplateContext context(6, 128);
    TEST_ASSERT_TRUE_MESSAGE(context.isReady(), "Constrained web context should allocate");
    context.setRegistry(DeviceFrameworkTemplatePlaceholders::getRegistry());
    TemplateRenderer::initializeContext(context, templateData);

    size_t bytesRendered = 0;
    for (size_t chunk = 0; chunk < 2048 && !TemplateRenderer::isComplete(context) &&
         !TemplateRenderer::hasError(context); ++chunk) {
        bytesRendered += TemplateRenderer::renderNextChunk(context, buffer, bufferSize);
    }

    if (TemplateRenderer::hasError(context)) {
        String message = String(description) + "; renderer error after " +
            String(bytesRendered) + " bytes";
        TEST_FAIL_MESSAGE(message.c_str());
        return;
    }
    if (!TemplateRenderer::isComplete(context)) {
        String message = String(description) + "; renderer stalled after " +
            String(bytesRendered) + " bytes in " + context.getStateString() +
            " at depth " + String(context.renderingDepth);
        TEST_FAIL_MESSAGE(message.c_str());
        return;
    }
    TEST_ASSERT_FALSE_MESSAGE(TemplateRenderer::hasError(context), description);
    TEST_ASSERT_TRUE_MESSAGE(TemplateRenderer::isComplete(context), description);
    TEST_ASSERT_TRUE_MESSAGE(bytesRendered > 512, description);
}

void assertTemplateCompletes(const char* templateData, const char* description) {
    uint8_t buffer[128];
    assertTemplateCompletesIntoBuffer(templateData, description, buffer, sizeof(buffer));
}

void assertTemplateCompletesWithAsyncSizedChunks(const char* templateData,
                                                 const char* description) {
    // ESPAsyncWebServer supplies up to 1,064 payload bytes after chunk framing
    // on ESP8266 (two 536-byte TCP MSS buffers minus eight framing bytes).
    assertTemplateCompletesIntoBuffer(
        templateData, description, asyncWebChunkBuffer, sizeof(asyncWebChunkBuffer));
}

void assertTemplateContainsMarkers(const char* templateData, const char* description,
                                   const char* const* markers, size_t markerCount) {
    constexpr size_t kWindowCapacity = 256;
    constexpr size_t kMaxMarkers = 16;
    bool found[kMaxMarkers] = {};
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(kMaxMarkers, markerCount);

    TemplateContext context(6, 128);
    TEST_ASSERT_TRUE_MESSAGE(context.isReady(), "Constrained web context should allocate");
    context.setRegistry(DeviceFrameworkTemplatePlaceholders::getRegistry());
    TemplateRenderer::initializeContext(context, templateData);

    uint8_t chunk[128];
    char window[kWindowCapacity + 1] = {};
    size_t windowLength = 0;
    for (size_t iterations = 0; iterations < 2048 && !TemplateRenderer::isComplete(context) &&
         !TemplateRenderer::hasError(context); ++iterations) {
        const size_t written = TemplateRenderer::renderNextChunk(context, chunk, sizeof(chunk));
        if (written > 0) {
            const size_t keep = written > kWindowCapacity ? kWindowCapacity : written;
            if (windowLength + keep > kWindowCapacity) {
                const size_t discard = windowLength + keep - kWindowCapacity;
                memmove(window, window + discard, windowLength - discard);
                windowLength -= discard;
            }
            memcpy(window + windowLength, chunk + written - keep, keep);
            windowLength += keep;
            window[windowLength] = '\0';
            for (size_t marker = 0; marker < markerCount; ++marker) {
                found[marker] = found[marker] || strstr(window, markers[marker]) != nullptr;
            }
        }
    }

    TEST_ASSERT_FALSE_MESSAGE(TemplateRenderer::hasError(context), description);
    TEST_ASSERT_TRUE_MESSAGE(TemplateRenderer::isComplete(context), description);
    for (size_t marker = 0; marker < markerCount; ++marker) {
        TEST_ASSERT_TRUE_MESSAGE(found[marker], markers[marker]);
    }
}

void assertStatusJSONStreamsInSmallChunks() {
    const uint32_t originalCacheInterval = getConfigAPIStatusCacheInterval();
    const String originalDeviceName = DeviceFramework::getDeviceName();
    setConfigAPIStatusCacheInterval(0);
    DeviceFramework::setDeviceName("Status \"quoted\"");
    DeviceStatusManager::updateRuntimeInfo();
    setConfigAPIStatusCacheInterval(originalCacheInterval);
    DeviceFramework::setDeviceName(originalDeviceName.c_str());
    CountingPrintAdapter counter;
    DeviceStatusManager::buildJSONResponse(counter, DeviceStatusManager::getStatus());

    DeviceStatusManager::JSONStreamState stream;
    DeviceStatusManager::resetJSONStreamState(stream);
    uint8_t chunk[37];
    size_t streamedBytes = 0;
    uint8_t firstByte = 0;
    static char serializedJSON[1536];
    memset(serializedJSON, 0, sizeof(serializedJSON));
    uint8_t lastByte = 0;

    const char* const expectedMarkers[] = {
        "\"name\":\"Status \\\"quoted\\\"\"",
        "\"free_heap_delta\":",
        "\"highest_free\":",
        "\"lowest_free\":",
        "\"status_update_count\":",
        "\"last_status_update_ms\":",
        "\"free_heap_trend_bytes_per_minute\":",
    };
    bool foundMarkers[sizeof(expectedMarkers) / sizeof(expectedMarkers[0])] = {};
    bool foundLegacyMetricName = false;
    char markerWindow[193] = {};
    size_t markerWindowLength = 0;

    for (size_t iteration = 0; iteration < 256 && !stream.complete; ++iteration) {
        const size_t written = DeviceStatusManager::renderJSONChunk(
            stream, chunk, sizeof(chunk), DeviceStatusManager::getStatus());
        TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, written,
            "JSON stream must make progress until complete");
        TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(sizeof(serializedJSON) - 1, streamedBytes + written,
            "Serialized status must fit the parser regression buffer");
        memcpy(serializedJSON + streamedBytes, chunk, written);
        if (streamedBytes == 0) firstByte = chunk[0];
        lastByte = chunk[written - 1];
        streamedBytes += written;
        const size_t retained = written > sizeof(markerWindow) - 1 ? sizeof(markerWindow) - 1 : written;
        if (markerWindowLength + retained > sizeof(markerWindow) - 1) {
            const size_t discard = markerWindowLength + retained - (sizeof(markerWindow) - 1);
            memmove(markerWindow, markerWindow + discard, markerWindowLength - discard);
            markerWindowLength -= discard;
        }
        memcpy(markerWindow + markerWindowLength, chunk + written - retained, retained);
        markerWindowLength += retained;
        markerWindow[markerWindowLength] = '\0';
        for (size_t marker = 0; marker < sizeof(expectedMarkers) / sizeof(expectedMarkers[0]); ++marker) {
            foundMarkers[marker] = foundMarkers[marker] || strstr(markerWindow, expectedMarkers[marker]) != nullptr;
        }
        foundLegacyMetricName = foundLegacyMetricName ||
            strstr(markerWindow, "\"peak_usage\":") != nullptr ||
            strstr(markerWindow, "\"loop_count\":") != nullptr;
    }
    serializedJSON[streamedBytes] = '\0';
    JsonDocument parsedStatus;
    const DeserializationError parseError = deserializeJson(parsedStatus, serializedJSON);
    TEST_ASSERT_FALSE_MESSAGE(parseError, parseError.c_str());

    TEST_ASSERT_TRUE_MESSAGE(stream.complete, "JSON stream should complete in bounded chunks");
    TEST_ASSERT_EQUAL_UINT32(counter.getCount(), streamedBytes);
    TEST_ASSERT_EQUAL_HEX8(0x7B, firstByte);
    TEST_ASSERT_EQUAL_HEX8(0x7D, lastByte);
    for (size_t marker = 0; marker < sizeof(expectedMarkers) / sizeof(expectedMarkers[0]); ++marker) {
        TEST_ASSERT_TRUE_MESSAGE(foundMarkers[marker], expectedMarkers[marker]);
    }
    TEST_ASSERT_FALSE_MESSAGE(foundLegacyMetricName, "Legacy telemetry names must not be emitted");
}

} // namespace

// Test WebInterface methods
void test_web_interface_methods() {
    // Test WebInterface enabled status - should be true since it's running in test mode
    bool enabled = DeviceFrameworkWeb::isEnabled();
    TEST_ASSERT_TRUE_MESSAGE(enabled,
        "WebInterface should be enabled in test mode");

    // Test config mode status
    bool configMode = DeviceFrameworkWeb::isInConfigMode();

    // WebInterface config mode depends on WiFi connection status
    bool wifiConnected = WiFi.isConnected();
    if (wifiConnected) {
        TEST_ASSERT_FALSE_MESSAGE(configMode,
            "WebInterface should not be in config mode when WiFi connects");
    } else {
        TEST_ASSERT_TRUE_MESSAGE(configMode,
            "WebInterface should be in config mode when WiFi fails");
    }

    assertTemplateCompletes(base_template,
        "Root web template should finish rendering without an error");
    assertTemplateCompletesWithAsyncSizedChunks(base_template,
        "Root web template should finish with ESPAsyncWebServer-sized chunks");
    assertTemplateCompletes(error404_template,
        "404 web template should finish rendering without an error");
    const char* const uiMarkers[] = {
        "DeviceFramework UI Test",
        "Test Lab",
        "df-web-theme",
        "--df-accent:#15803d;",
        "alt=\"Test Lab\"",
        "href=\"/assets/deviceframework.css\"",
        "src=\"/assets/deviceframework.js\"",
        "src=\"/assets/deviceframework-logo\"",
        "href=\"#about\"",
        "id=\"about\"",
        "https://example.test",
        "noopener noreferrer",
    };
    assertTemplateContainsMarkers(base_template,
        "Root web template should contain configured UI markers",
        uiMarkers, sizeof(uiMarkers) / sizeof(uiMarkers[0]));

    assertStatusJSONStreamsInSmallChunks();

    // Test restart doesn't crash
    DeviceFrameworkWeb::restart();
    // Async listener rebinding completes on the network scheduler.
    delay(1000);

    // Test methods still work after restart
    bool enabledAfter = DeviceFrameworkWeb::isEnabled();
    bool configModeAfter = DeviceFrameworkWeb::isInConfigMode();

    // After restart, WebInterface should still be enabled
    TEST_ASSERT_TRUE_MESSAGE(enabledAfter,
        "WebInterface should still be enabled after restart");
    // Config mode should match WiFi connection status
    if (wifiConnected) {
        TEST_ASSERT_FALSE_MESSAGE(configModeAfter,
            "WebInterface should not be in config mode when WiFi connects");
    } else {
        TEST_ASSERT_TRUE_MESSAGE(configModeAfter,
            "WebInterface should be in config mode when WiFi fails");
    }
}
