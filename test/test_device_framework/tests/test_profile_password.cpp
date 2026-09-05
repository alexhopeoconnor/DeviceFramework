#include <unity.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <DeviceFramework.h>
#include <Provisioning/DeviceFrameworkProvisioning.h>

#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
#include <DeviceFrameworkLocalProfile.h>
#endif

void test_profile_password_is_persistent_without_reprovisioning() {
#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
    const String originalPassword(DeviceFramework::getDevicePassword());
    TEST_ASSERT_TRUE_MESSAGE(DeviceFramework::setDevicePassword("runtime-rotated-password"),
        "A runtime password should persist in the V4 transaction");

    // Simulate a process restart: RAM state is lost, then the committed V4
    // record restores it before provisioning evaluates the selected profile.
    setConfigDevicePassword("");
    DeviceFrameworkStorageLoadResult loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(), "The V4 record should reload");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("runtime-rotated-password", DeviceFramework::getDevicePassword(),
        "Storage must restore the runtime-rotated password");

    const bool firstApplication = DeviceFrameworkProvisioning::apply(loaded);
#if DEVICEFRAMEWORK_PROFILE_IS_RECONCILE
    // Earlier storage tests deliberately clear the recorded provisioning state.
    // A reconcile profile must seed its explicit password once in that case.
    TEST_ASSERT_TRUE_MESSAGE(firstApplication,
        "A reconcile profile must apply after its recorded state was cleared");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(DEVICEFRAMEWORK_PROFILE_DEVICE_PASSWORD,
        DeviceFramework::getDevicePassword(),
        "A reconcile profile must apply its explicit shared password");
    TEST_ASSERT_TRUE_MESSAGE(DeviceFramework::setDevicePassword("runtime-rotated-password"),
        "A later runtime password rotation should persist after reconciliation");
    setConfigDevicePassword("");
    loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(),
        "The post-reconcile runtime password should reload");
#else
    TEST_ASSERT_FALSE_MESSAGE(firstApplication,
        "A bootstrap profile must not reapply a valid stored configuration");
#endif

    TEST_ASSERT_FALSE_MESSAGE(DeviceFrameworkProvisioning::apply(loaded),
        "The same selected profile must not overwrite a later runtime password");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("runtime-rotated-password",
        DeviceFramework::getDevicePassword(),
        "A selected profile must not overwrite a valid runtime password");
    TEST_ASSERT_TRUE_MESSAGE(DeviceFramework::setDevicePassword(originalPassword.c_str()),
        "The fixture password should restore for later network tests");
#else
    TEST_IGNORE_MESSAGE("Profile password regression requires the profile fixture environment");
#endif
}
