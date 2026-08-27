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
    const DeviceFrameworkStorageLoadResult loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(), "The V4 record should reload");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("runtime-rotated-password", DeviceFramework::getDevicePassword(),
        "Storage must restore the runtime-rotated password");
    TEST_ASSERT_FALSE_MESSAGE(
        DeviceFrameworkProvisioning::apply(loaded),
        "A bootstrap profile must not reapply a valid stored configuration"
    );
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "runtime-rotated-password",
        DeviceFramework::getDevicePassword(),
        "A selected profile must not overwrite a valid runtime password"
    );
    TEST_ASSERT_TRUE_MESSAGE(DeviceFramework::setDevicePassword(originalPassword.c_str()),
        "The fixture password should restore for later network tests");
#else
    TEST_IGNORE_MESSAGE("Profile password regression requires the profile fixture environment");
#endif
}
