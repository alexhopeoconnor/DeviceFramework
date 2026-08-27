#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <Storage/DeviceFrameworkStorage.h>

void test_eeprom_storage_methods() {
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(64, getConfigEEPROMStart(), "EEPROM start should remain reserved");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1024, getConfigEEPROMSize(), "V4 uses two transactional slots");

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Storage reset should commit");
    const DeviceFrameworkStorageLoadResult result = DeviceFrameworkStorage::load();
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(DeviceFrameworkStorageLoadStatus::Empty),
                              static_cast<int>(result.status),
                              "Erased storage should be reported as empty");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(4, DeviceFrameworkStorage::STORAGE_FORMAT_VERSION,
                                   "Storage format must be V4");
}

void test_storage_legacy_markers_allow_profile_cutover() {
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Legacy-marker test should start with empty storage");

    const uint32_t legacyMagic = 0x44464333UL;
    const uint16_t base = getConfigEEPROMStart();
    for (uint8_t index = 0; index < sizeof(legacyMagic); ++index) {
        EEPROM.write(base + index, static_cast<uint8_t>(legacyMagic >> (index * 8)));
    }
    EEPROM.commit();

    const DeviceFrameworkStorageLoadResult result = DeviceFrameworkStorage::load();
    TEST_ASSERT_EQUAL_MESSAGE(
        static_cast<int>(DeviceFrameworkStorageLoadStatus::UnsupportedLegacyFormat),
        static_cast<int>(result.status),
        "A DFC3 marker must permit an explicit profile to replace it without an erase firmware"
    );

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Legacy-marker test should clean up storage");
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "Legacy-marker test should restore a valid V4 record");
}
