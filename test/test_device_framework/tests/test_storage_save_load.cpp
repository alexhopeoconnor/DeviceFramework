#include <unity.h>
#include <Arduino.h>
#include <DeviceFramework.h>
#include <Storage/DeviceFrameworkStorage.h>

void test_storage_save_load() {
    Serial.println("[TEST]   Testing V2 storage save/load/reset...");
    const String originalDeviceName(DeviceFramework::getDeviceName());
    const String originalMqttServer(DeviceFramework::getMqttServer());
    const uint16_t originalMqttPort = DeviceFramework::getMqttPort();

    DeviceFramework::setDeviceName("StorageTestDevice");
    DeviceFramework::setMqttServer("storage.test.server");
    DeviceFramework::setMqttPort(9999);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(), "V2 storage save should succeed");

    DeviceFramework::setDeviceName("ChangedDevice");
    DeviceFramework::setMqttServer("changed.server");
    DeviceFramework::setMqttPort(8888);

    const DeviceFrameworkStorageLoadResult loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(), "Saved V2 slot should load");
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
    Serial.println("[TEST]   V2 storage save/load/reset tests completed.");
}
