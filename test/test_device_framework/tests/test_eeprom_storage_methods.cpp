#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <Storage/DeviceFrameworkStorage.h>

// Test EEPROMStorage methods
void test_eeprom_storage_methods() {
    // Test EEPROM configuration - should return expected default values
    uint16_t eepromStart = getConfigEEPROMStart();
    TEST_ASSERT_EQUAL_MESSAGE(64, eepromStart,
        "EEPROM start should be 64 (CONFIG_eepromStart)");

    uint16_t eepromSize = getConfigEEPROMSize();
    TEST_ASSERT_EQUAL_MESSAGE(512, eepromSize,
        "EEPROM size should be 512 (CONFIG_eepromSize)");

    // Test config version - should return expected default value
    const char* configVersion = getConfigVersion();
    TEST_ASSERT_NOT_NULL_MESSAGE(configVersion,
        "Config version should not be NULL");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("V1.0", configVersion,
        "Config version should be 'V1.0' (CONFIG_version)");

    // Test version validation - should be true after EEPROM is initialized in testing mode
    bool versionValid = DeviceFrameworkStorage::isVersionValid();
    TEST_ASSERT_TRUE_MESSAGE(versionValid,
        "Version should be valid after testing mode initialization");

    // Test that we can read the version from EEPROM directly
    char storedVersion[6] = {0};
    for (size_t i = 0; i < 5; i++) { // "V1.0" is 4 chars + null terminator
        storedVersion[i] = EEPROM.read(64 + i);
    }
    TEST_ASSERT_EQUAL_STRING_MESSAGE("V1.0", storedVersion,
        "Stored version in EEPROM should be 'V1.0'");
}
