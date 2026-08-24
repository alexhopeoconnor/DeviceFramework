#include <unity.h>
#include <Arduino.h>
#include <Utils/HostnameUtils.h>
#include <DeviceFrameworkPlatform.h>  // Platform abstraction for defaults

// Test HostnameUtils functionality
void test_hostname_utils() {
    Serial.println("[TEST]   Testing HostnameUtils functionality...");

    // Test sanitizeHostname with various inputs
    String result1 = HostnameUtils::sanitizeHostname("My Device Name");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("my-device-name", result1.c_str(),
        "Should sanitize spaces and convert to lowercase");

    String result2 = HostnameUtils::sanitizeHostname("Device_With_Underscores");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("device-with-underscores", result2.c_str(),
        "Should replace underscores with hyphens");

    String result3 = HostnameUtils::sanitizeHostname("Device-With-Hyphens");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("device-with-hyphens", result3.c_str(),
        "Should preserve hyphens");

    String result4 = HostnameUtils::sanitizeHostname("Device.With.Dots");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("device-with-dots", result4.c_str(),
        "Should replace dots with hyphens");

    String result5 = HostnameUtils::sanitizeHostname("123Device");
    TEST_ASSERT_TRUE_MESSAGE(result5.startsWith("esp-"),
        "Should prefix with 'esp-' when starting with number");

    String result6 = HostnameUtils::sanitizeHostname("Device!!!@#$%");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("device", result6.c_str(),
        "Should remove invalid characters");

    String result7 = HostnameUtils::sanitizeHostname("   Device   ");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("device", result7.c_str(),
        "Should trim leading/trailing spaces and hyphens");

    // Test sanitizeHostname returns empty for empty/null (pure sanitization function)
    String result8 = HostnameUtils::sanitizeHostname("");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", result8.c_str(),
        "Should return empty string for empty input");

    String result9 = HostnameUtils::sanitizeHostname(nullptr);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", result9.c_str(),
        "Should return empty string for null input");

    // Test isValidHostname
    TEST_ASSERT_TRUE_MESSAGE(HostnameUtils::isValidHostname("valid-hostname"),
        "Should validate correct hostname");

    TEST_ASSERT_TRUE_MESSAGE(HostnameUtils::isValidHostname("hostname123"),
        "Should validate hostname with numbers");

    TEST_ASSERT_FALSE_MESSAGE(HostnameUtils::isValidHostname(""),
        "Should reject empty hostname");

    TEST_ASSERT_FALSE_MESSAGE(HostnameUtils::isValidHostname(nullptr),
        "Should reject null hostname");

    TEST_ASSERT_FALSE_MESSAGE(HostnameUtils::isValidHostname("hostname with spaces"),
        "Should reject hostname with spaces");

    TEST_ASSERT_FALSE_MESSAGE(HostnameUtils::isValidHostname("hostname@invalid"),
        "Should reject hostname with invalid characters");

    // Test getSanitizedHostname with fallback
    String result10 = HostnameUtils::getSanitizedHostname("Valid Input", "fallback");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("valid-input", result10.c_str(),
        "Should sanitize valid input");

    String result11 = HostnameUtils::getSanitizedHostname("", "fallback");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("fallback", result11.c_str(),
        "Should use fallback for empty input");

    String result12 = HostnameUtils::getSanitizedHostname(nullptr, "fallback");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("fallback", result12.c_str(),
        "Should use fallback for null input");

    String result13 = HostnameUtils::getSanitizedHostname("", nullptr);
    String expectedDefaultFallback = HostnameUtils::sanitizeHostname(DF_DEFAULT_DEVICE_NAME);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedDefaultFallback.c_str(), result13.c_str(),
        "Should use sanitized default device name when no fallback provided");

    Serial.println("[TEST]   HostnameUtils tests completed successfully");
}
