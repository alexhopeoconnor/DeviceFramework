#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <Storage/DeviceFrameworkStorage.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <EEPROM.h>
#include <Configuration/DeviceFrameworkParameters.h>

// Test Storage module save/load/clear operations
void test_storage_save_load() {
    Serial.println("[TEST]   Testing Storage module save/load/clear operations...");

    // Store original values to restore later
    String originalDeviceName = DeviceFramework::getDeviceName() ? String(DeviceFramework::getDeviceName()) : String("");
    String originalMqttServer = DeviceFramework::getMqttServer() ? String(DeviceFramework::getMqttServer()) : String("");
    uint16_t originalMqttPort = DeviceFramework::getMqttPort();

    // Set test values
    DeviceFramework::setDeviceName("StorageTestDevice");
    DeviceFramework::setMqttServer("storage.test.server");
    DeviceFramework::setMqttPort(9999);

    // Save parameters to EEPROM
    DeviceFramework::saveParameters();

    // Verify version is written to EEPROM
    uint16_t eepromStart = getConfigEEPROMStart();
    const char* expectedVersion = getConfigVersion();
    size_t versionLen = strlen(expectedVersion) + 1; // +1 for null terminator

    char storedVersion[versionLen];
    memset(storedVersion, 0, versionLen);
    for (size_t i = 0; i < versionLen; i++) {
        storedVersion[i] = EEPROM.read(eepromStart + i);
    }
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedVersion, storedVersion,
        "Version should be stored in EEPROM after save()");

    // Change values in memory (not saved yet)
    DeviceFramework::setDeviceName("ChangedDevice");
    DeviceFramework::setMqttServer("changed.server");
    DeviceFramework::setMqttPort(8888);

    // Verify values changed in memory
    TEST_ASSERT_EQUAL_STRING_MESSAGE("ChangedDevice", DeviceFramework::getDeviceName(),
        "Device name should be changed in memory");

    // Load parameters from EEPROM
    DeviceFramework::loadParameters();

    // Verify values were loaded from EEPROM (should match saved values)
    TEST_ASSERT_EQUAL_STRING_MESSAGE("StorageTestDevice", DeviceFramework::getDeviceName(),
        "Device name should be loaded from EEPROM");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("storage.test.server", DeviceFramework::getMqttServer(),
        "MQTT server should be loaded from EEPROM");
    TEST_ASSERT_EQUAL_MESSAGE(9999, DeviceFramework::getMqttPort(),
        "MQTT port should be loaded from EEPROM");

    // Test clear() - should clear EEPROM and restore defaults
    DeviceFrameworkStorage::clear();

    // Verify version is cleared (should be 0xFF or null)
    char clearedVersion[versionLen];
    bool isCleared = true;
    for (size_t i = 0; i < versionLen; i++) {
        clearedVersion[i] = EEPROM.read(eepromStart + i);
        // Check if it's cleared (0xFF or null terminator)
        if (clearedVersion[i] != 0xFF && (i == 0 || clearedVersion[i] != '\0')) {
            isCleared = false;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(isCleared,
        "EEPROM should be cleared after clear()");

    // Test version mismatch handling - write invalid version
    String invalidVersion = "V9.9";
    for (size_t i = 0; i < invalidVersion.length() + 1; i++) {
        EEPROM.write(eepromStart + i, i < invalidVersion.length() ? invalidVersion[i] : '\0');
    }
    EEPROM.commit();

    // Load should detect version mismatch and restore defaults
    DeviceFrameworkStorage::load();

    // After version mismatch, should have default values
    // Verify version is now valid
    bool versionValidAfterMismatch = DeviceFrameworkStorage::isVersionValid();
    TEST_ASSERT_TRUE_MESSAGE(versionValidAfterMismatch,
        "Version should be valid after version mismatch recovery");

    // Restore original values
    DeviceFramework::setDeviceName(originalDeviceName.c_str());
    DeviceFramework::setMqttServer(originalMqttServer.c_str());
    DeviceFramework::setMqttPort(originalMqttPort);
    DeviceFramework::saveParameters();

    Serial.println("[TEST]   Storage module save/load/clear tests completed successfully");
}
