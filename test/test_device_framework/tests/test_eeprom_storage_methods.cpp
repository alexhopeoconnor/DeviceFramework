#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <Storage/DeviceFrameworkStorage.h>

void test_eeprom_storage_methods() {
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(64, getConfigEEPROMStart(), "EEPROM start should remain reserved");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1024, getConfigEEPROMSize(), "V3 uses two transactional slots");

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Storage reset should commit");
    const DeviceFrameworkStorageLoadResult result = DeviceFrameworkStorage::load();
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(DeviceFrameworkStorageLoadStatus::Empty),
                              static_cast<int>(result.status),
                              "Erased storage should be reported as empty");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(3, DeviceFrameworkStorage::STORAGE_FORMAT_VERSION,
                                   "Storage format must be V3");
}
