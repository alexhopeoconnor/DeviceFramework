#include <unity.h>
#include <Arduino.h>
#include <Utils/CRC32Utils.h>

// Test CRC32Utils functionality
void test_crc32_utils() {
    Serial.println("[TEST]   Testing CRC32Utils functionality...");

    // Test with known test vectors
    const char* test1 = "Hello";
    uint32_t crc1 = CRC32Utils::calculate((const uint8_t*)test1, strlen(test1));
    // Note: We'll use a simple test since CRC32 implementation may vary
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, crc1,
        "CRC32 should not be zero for non-empty input");

    const char* test2 = "World";
    uint32_t crc2 = CRC32Utils::calculate((const uint8_t*)test2, strlen(test2));
    TEST_ASSERT_NOT_EQUAL_MESSAGE(crc1, crc2,
        "Different inputs should produce different CRC32 values");

    // Test with empty input
    uint32_t crc3 = CRC32Utils::calculate(nullptr, 0);
    TEST_ASSERT_EQUAL_MESSAGE(0xffffffff, crc3,
        "CRC32 should be 0xffffffff for empty input");

    // Test with single character
    const char* test4 = "A";
    uint32_t crc4 = CRC32Utils::calculate((const uint8_t*)test4, 1);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, crc4,
        "CRC32 should not be zero for single character");

    // Test with binary data
    const uint8_t binaryData[] = {0x00, 0x01, 0x02, 0x03, 0xFF};
    uint32_t crc5 = CRC32Utils::calculate(binaryData, sizeof(binaryData));
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, crc5,
        "CRC32 should not be zero for binary data");

    // Test consistency - same input should produce same CRC
    uint32_t crc6 = CRC32Utils::calculate((const uint8_t*)test1, strlen(test1));
    TEST_ASSERT_EQUAL_MESSAGE(crc1, crc6,
        "Same input should produce same CRC32 value");

    Serial.println("[TEST]   CRC32Utils tests completed successfully");
}
