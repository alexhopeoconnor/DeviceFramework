#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <UI/DeviceFrameworkUI.h>

namespace {
String toString(const DeviceFrameworkText& text) {
    String value;
    for (size_t i = 0; i < text.length(); ++i) value += text.at(i);
    return value;
}
}

void test_device_framework_ui_configuration() {
    Serial.println("[TEST]   Testing DeviceFramework UI configuration...");

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkUI::isConfigured(),
        "The test firmware should configure its product UI before setup");
    TEST_ASSERT_EQUAL_STRING("Test Lab", toString(DeviceFrameworkUI::getBrandName()).c_str());
    TEST_ASSERT_EQUAL_STRING("DeviceFramework UI Test", toString(DeviceFrameworkUI::getWebTitle()).c_str());
    TEST_ASSERT_EQUAL_STRING("Test Lab", toString(DeviceFrameworkUI::getLogoAltText()).c_str());

    const char* stylesheet = DeviceFrameworkUI::getWebThemeStyle();
    TEST_ASSERT_NOT_NULL(stylesheet);
    TEST_ASSERT_NOT_EQUAL(-1, String(stylesheet).indexOf("id='df-web-theme'"));
    TEST_ASSERT_NOT_EQUAL(-1, String(stylesheet).indexOf("--df-accent:#15803d;"));
    TEST_ASSERT_NOT_EQUAL(-1, String(DeviceFrameworkUI::getAboutNavigation()).indexOf("#about"));
    const String about = DeviceFrameworkUI::getAboutSection();
    TEST_ASSERT_NOT_EQUAL(-1, about.indexOf("fixed product About"));
    TEST_ASSERT_NOT_EQUAL(-1, about.indexOf("https://example.test"));
    TEST_ASSERT_NOT_EQUAL(-1, about.indexOf("rel=\"noopener noreferrer\""));

    DeviceFrameworkUIConfig lateConfig;
    lateConfig.branding.webTitle = DeviceFrameworkText::ram("Late UI change");
    TEST_ASSERT_FALSE_MESSAGE(DeviceFramework::setUIConfig(lateConfig),
        "Product UI must be immutable after DeviceFramework starts WiFi services");

    Serial.println("[TEST]   DeviceFramework UI configuration test completed successfully");
}
