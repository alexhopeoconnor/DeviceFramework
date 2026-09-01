#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <Storage/DeviceFrameworkStorage.h>
#include <EEPROM.h>

void test_storage_save_load() {
    Serial.println("[TEST]   Testing V4 storage save/load/reset...");
    const String originalDeviceName(DeviceFramework::getDeviceName());
    const String originalMqttServer(DeviceFramework::getMqttServer());
    const uint16_t originalMqttPort = DeviceFramework::getMqttPort();

    DeviceFramework::setDeviceName("StorageTestDevice");
    DeviceFramework::setMqttServer("storage.test.server");
    DeviceFramework::setMqttPort(9999);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "V4 storage save should succeed");

    DeviceFramework::setDeviceName("ChangedDevice");
    DeviceFramework::setMqttServer("changed.server");
    DeviceFramework::setMqttPort(8888);

    const DeviceFrameworkStorageLoadResult loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(), "Saved V4 slot should load");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("StorageTestDevice", DeviceFramework::getDeviceName(), "Device name should reload");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("storage.test.server", DeviceFramework::getMqttServer(), "MQTT server should reload");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(9999, DeviceFramework::getMqttPort(), "MQTT port should reload");

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Reset should erase both slots");
    const DeviceFrameworkStorageLoadResult empty = DeviceFrameworkStorage::load();
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(DeviceFrameworkStorageLoadStatus::Empty),
                              static_cast<int>(empty.status),
                              "Reset storage should be empty");

    DeviceFramework::setDeviceName(originalDeviceName.c_str());
    DeviceFramework::setMqttServer(originalMqttServer.c_str());
    DeviceFramework::setMqttPort(originalMqttPort);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "Restoring test configuration should save");
    Serial.println("[TEST]   V4 storage save/load/reset tests completed.");
}

void test_storage_falls_back_to_prior_valid_slot() {
    const String originalDeviceName(DeviceFramework::getDeviceName());
    const String originalMqttServer(DeviceFramework::getMqttServer());
    const uint16_t originalMqttPort = DeviceFramework::getMqttPort();
    const String originalPassword(DeviceFramework::getDevicePassword());

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Fallback test should start with empty storage");
    DeviceFramework::setDeviceName("transaction-first");
    DeviceFramework::setMqttServer("first.storage.test");
    DeviceFramework::setMqttPort(1883);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "First transactional record should save");

    DeviceFramework::setDeviceName("transaction-second");
    DeviceFramework::setMqttServer("second.storage.test");
    DeviceFramework::setMqttPort(2883);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "Second transactional record should save");

    const uint16_t slotSize = (getConfigEEPROMSize() - getConfigEEPROMStart()) / 2;
    const uint16_t newestPayloadByte = getConfigEEPROMStart() + slotSize + 40;
    EEPROM.write(newestPayloadByte, EEPROM.read(newestPayloadByte) ^ 0x01);
    TEST_ASSERT_TRUE_MESSAGE(EEPROM.commit(), "Newest-slot corruption should commit for fallback test");

    const DeviceFrameworkStorageLoadResult loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(),
        "The older valid transactional slot should remain loadable");
    TEST_ASSERT_EQUAL_STRING("transaction-first", DeviceFramework::getDeviceName());
    TEST_ASSERT_EQUAL_STRING("first.storage.test", DeviceFramework::getMqttServer());
    TEST_ASSERT_EQUAL_UINT16(1883, DeviceFramework::getMqttPort());

    DeviceFramework::setDeviceName(originalDeviceName.c_str());
    DeviceFramework::setMqttServer(originalMqttServer.c_str());
    DeviceFramework::setMqttPort(originalMqttPort);
    TEST_ASSERT_TRUE_MESSAGE(setConfigDevicePassword(originalPassword.c_str()),
        "Fallback test should restore the runtime password before saving");
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(),
        "Fallback test should restore a valid transactional record");
}

void test_storage_foreign_application_is_distinguished() {
    const DeviceFrameworkApplicationIdentity originalIdentity = DeviceFramework::getApplicationIdentity();

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Foreign-application test should start with empty storage");
    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFramework::configureApplication("storage-source", "1.0.0", 1),
        "Source application identity should be valid"
    );
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "Source application configuration should save");

    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFramework::configureApplication("storage-target", "1.0.0", 1),
        "Target application identity should be valid"
    );
    const DeviceFrameworkStorageLoadResult foreign = DeviceFrameworkStorage::load();
    TEST_ASSERT_EQUAL_MESSAGE(
        static_cast<int>(DeviceFrameworkStorageLoadStatus::ForeignApplication),
        static_cast<int>(foreign.status),
        "A valid V4 record for another application must not be reported as corrupt"
    );
    TEST_ASSERT_FALSE_MESSAGE(foreign.hasUsableConfiguration(), "Foreign application values must not be loaded");

    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFramework::configureApplication(
            originalIdentity.applicationId.c_str(), originalIdentity.firmwareVersion.c_str(),
            originalIdentity.configurationSchema, originalIdentity.migration
        ),
        "Original application identity should restore"
    );
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "Original test configuration should restore");
}

void test_storage_incompatible_schema_retains_device_password() {
    const DeviceFrameworkApplicationIdentity originalIdentity = DeviceFramework::getApplicationIdentity();
    const String originalPassword(DeviceFramework::getDevicePassword());
    const uint16_t newerSchema = static_cast<uint16_t>(originalIdentity.configurationSchema + 1);

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Schema-password test should start with empty storage");
    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFramework::configureApplication(
            originalIdentity.applicationId.c_str(), originalIdentity.firmwareVersion.c_str(), newerSchema
        ),
        "Newer test schema identity should be valid"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFramework::setDevicePassword("schema-password"),
        "A valid password should save in the newer V4 record"
    );

    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFramework::configureApplication(
            originalIdentity.applicationId.c_str(), originalIdentity.firmwareVersion.c_str(),
            originalIdentity.configurationSchema, originalIdentity.migration
        ),
        "Original schema identity should restore"
    );
    TEST_ASSERT_TRUE_MESSAGE(setConfigDevicePassword(""), "Test should clear only runtime password state");

    const DeviceFrameworkStorageLoadResult incompatible = DeviceFrameworkStorage::load();
    TEST_ASSERT_EQUAL_MESSAGE(
        static_cast<int>(DeviceFrameworkStorageLoadStatus::Incompatible),
        static_cast<int>(incompatible.status),
        "A newer V4 schema should remain incompatible"
    );
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "schema-password", DeviceFramework::getDevicePassword(),
        "A valid same-application V4 password must survive incompatible schema handling"
    );

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::reset(), "Schema-password test cleanup should reset storage");
    TEST_ASSERT_TRUE_MESSAGE(
        DeviceFramework::setDevicePassword(originalPassword.c_str()),
        "Original password should restore after schema-password test"
    );
}
