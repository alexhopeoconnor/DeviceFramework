#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <WiFiClient.h>
#include <TemplateEngine.h>
#include <WebInterface/DeviceFrameworkTemplatePlaceholders.h>
#include <WebInterface/templates/WebInterfaceHTML.h>

namespace {

void assertTemplateCompletes(const char* templateData, const char* description) {
    TemplateContext context;
    context.setRegistry(DeviceFrameworkTemplatePlaceholders::getRegistry());
    TemplateRenderer::initializeContext(context, templateData);

    uint8_t buffer[128];
    size_t bytesRendered = 0;
    for (size_t chunk = 0; chunk < 2048 && !TemplateRenderer::isComplete(context) &&
         !TemplateRenderer::hasError(context); ++chunk) {
        bytesRendered += TemplateRenderer::renderNextChunk(context, buffer, sizeof(buffer));
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
    assertTemplateCompletes(error404_template,
        "404 web template should finish rendering without an error");

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
