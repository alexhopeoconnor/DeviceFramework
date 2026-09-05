#include <unity.h>

#include <cstring>

#include <Configuration/DeviceFrameworkParameters.h>
#include <DeviceFramework.h>
#include <Provisioning/DeviceFrameworkProvisioning.h>
#include <Storage/DeviceFrameworkStorage.h>

#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE
#include <DeviceFrameworkLocalProfile.h>
#endif

#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE && \
    DEVICEFRAMEWORK_PROFILE_IS_RECONCILE
namespace {
const char* profileParameterValue(const char* id) {
    for (size_t index = 0; index < DEVICEFRAMEWORK_PROFILE_PARAMETER_COUNT; ++index) {
        const DeviceFrameworkProvisionedParameter& parameter = DEVICEFRAMEWORK_PROFILE_PARAMETERS[index];
        if (strcmp(parameter.id, id) == 0) return parameter.value;
    }
    return nullptr;
}
}  // namespace
#endif

void test_reconcile_profile_updates_only_explicit_values_once() {
#if defined(DEVICEFRAMEWORK_HAS_LOCAL_PROFILE) && DEVICEFRAMEWORK_HAS_LOCAL_PROFILE && \
    DEVICEFRAMEWORK_PROFILE_IS_RECONCILE
    DeviceFrameworkParameterRegistry& registry = DeviceFrameworkParameters::getRegistry();
    const String originalDeviceName(DeviceFramework::getDeviceName());
    const String originalLogLevel(DeviceFrameworkParameters::getLogLevel());
    const String originalUnmanagedValue(registry.getValue("testdevicename"));
    const DeviceFrameworkProvisioningState originalState =
        DeviceFrameworkStorage::getProvisioningState();
    const char* expectedDeviceName = profileParameterValue("device");
    const char* expectedLogLevel = profileParameterValue("loglevel");
    TEST_ASSERT_NOT_NULL(expectedDeviceName);
    TEST_ASSERT_NOT_NULL(expectedLogLevel);

    // Start from a valid record and let the selected profile record its current
    // revision. This represents the first boot of the profiled firmware.
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(),
        "The profile fixture must have a valid persisted configuration");
    DeviceFrameworkStorageLoadResult loaded = DeviceFrameworkStorage::load();
    TEST_ASSERT_TRUE_MESSAGE(loaded.hasUsableConfiguration(),
        "The saved profile fixture configuration must reload");

    // Setup has already applied this fixture once. Model a board that was
    // running the preceding profile by changing only the recorded identity;
    // the selected firmware still provides the same current revision.
    DeviceFrameworkProvisioningState previous = DeviceFrameworkStorage::getProvisioningState();
    previous.profileHash ^= 0xFFFFFFFFUL;
    DeviceFrameworkStorage::setProvisioningState(previous);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkProvisioning::apply(loaded),
        "A prior profile identity must trigger reconciliation");

    // Simulate portal/API edits after that firmware version has been accepted.
    TEST_ASSERT_TRUE(registry.setValue("device", "User Device Name"));
    TEST_ASSERT_TRUE(registry.setValue("loglevel", "Verbose"));
    TEST_ASSERT_TRUE(registry.setValue("testdevicename", "Keep This Value"));
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(),
        "Runtime edits must persist before the next boot");
    loaded = DeviceFrameworkStorage::load();

    // Rebooting or reflashing the same profile revision must be a no-op.
    TEST_ASSERT_FALSE_MESSAGE(DeviceFrameworkProvisioning::apply(loaded),
        "The same reconcile revision must not overwrite later runtime edits");
    TEST_ASSERT_EQUAL_STRING("User Device Name", DeviceFramework::getDeviceName());
    TEST_ASSERT_EQUAL_STRING("Verbose", DeviceFrameworkParameters::getLogLevel());
    TEST_ASSERT_EQUAL_STRING("Keep This Value", registry.getValue("testdevicename").c_str());

    // A different profile identity owns its explicit values once as well.
    // Inverting the recorded hash precisely models uploading a profile with a
    // new id while retaining the same revision.
    previous = DeviceFrameworkStorage::getProvisioningState();
    previous.profileHash ^= 0xFFFFFFFFUL;
    DeviceFrameworkStorage::setProvisioningState(previous);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkProvisioning::apply(loaded),
        "A changed reconcile profile ID must apply its explicit managed values");
    TEST_ASSERT_EQUAL_STRING(expectedDeviceName, DeviceFramework::getDeviceName());
    TEST_ASSERT_EQUAL_STRING(expectedLogLevel, DeviceFrameworkParameters::getLogLevel());
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Keep This Value", registry.getValue("testdevicename").c_str(),
        "A profile ID change must preserve runtime values for omitted parameters");

    // A later runtime edit remains authoritative until the profile revision
    // changes. Persist it so the next load models the subsequent reboot.
    TEST_ASSERT_TRUE(registry.setValue("device", "User Changed Afterwards"));
    TEST_ASSERT_TRUE(registry.setValue("loglevel", "Verbose"));
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(),
        "Runtime edits after profile ID application must persist");
    loaded = DeviceFrameworkStorage::load();

    // A newer reconcile revision owns only the keys present in the profile.
    // Lowering the stored revision precisely models a device that was running
    // the preceding firmware before this profile-bearing image was uploaded.
    previous = DeviceFrameworkStorage::getProvisioningState();
    TEST_ASSERT_GREATER_THAN_UINT32(1, DEVICEFRAMEWORK_PROFILE_REVISION);
    previous.attemptedRevision = DEVICEFRAMEWORK_PROFILE_REVISION - 1U;
    DeviceFrameworkStorage::setProvisioningState(previous);

    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkProvisioning::apply(loaded),
        "A changed reconcile revision must apply its explicit managed values");
    TEST_ASSERT_EQUAL_STRING(expectedDeviceName, DeviceFramework::getDeviceName());
    TEST_ASSERT_EQUAL_STRING(expectedLogLevel, DeviceFrameworkParameters::getLogLevel());
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Keep This Value", registry.getValue("testdevicename").c_str(),
        "A profile revision change must preserve runtime values for omitted parameters");

    // Restore the independent fixture state for the remaining integration tests.
    TEST_ASSERT_TRUE(registry.setValue("device", originalDeviceName));
    TEST_ASSERT_TRUE(registry.setValue("loglevel", originalLogLevel));
    TEST_ASSERT_TRUE(registry.setValue("testdevicename", originalUnmanagedValue));
    DeviceFrameworkStorage::setProvisioningState(originalState);
    TEST_ASSERT_TRUE_MESSAGE(DeviceFrameworkStorage::save(),
        "Reconciliation test cleanup must restore a valid V4 record");
#else
    TEST_IGNORE_MESSAGE("Requires the reconcile profile fixture environment");
#endif
}
