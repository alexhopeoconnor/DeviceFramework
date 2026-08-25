#include <unity.h>
#include <Configuration/DeviceFrameworkConfig.h>
#include <Provisioning/DeviceFrameworkProvisioning.h>

#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
#include <DeviceFrameworkLocalProfile.h>
#endif

void test_profile_password_is_restored_without_reprovisioning() {
#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
    // A loaded record means bootstrap must leave stored WiFi and parameters
    // alone, but it must still restore the firmware's runtime password.
    setConfigDevicePassword("");
    const DeviceFrameworkStorageLoadResult loaded(DeviceFrameworkStorageLoadStatus::Loaded);
    TEST_ASSERT_FALSE_MESSAGE(
        DeviceFrameworkProvisioning::apply(loaded),
        "Bootstrap profile must not reprovision a matching stored configuration"
    );
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        DEVICEFRAMEWORK_PROFILE_DEVICE_PASSWORD,
        getConfigDevicePassword(),
        "Selected profile must restore the shared password on every boot"
    );
#else
    TEST_IGNORE_MESSAGE("Profile password regression requires the profile fixture environment");
#endif
}
