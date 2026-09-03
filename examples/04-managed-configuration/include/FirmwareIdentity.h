#pragma once

#include <DeviceFramework.h>

namespace FirmwareIdentity {
constexpr char APPLICATION_ID[] = "df-example-managed-configuration";
constexpr char FIRMWARE_VERSION[] = "1.0.0";
constexpr uint16_t CONFIGURATION_SCHEMA = 1;

inline bool configure() {
    return DeviceFramework::configureApplication(
        APPLICATION_ID, FIRMWARE_VERSION, CONFIGURATION_SCHEMA
    );
}
}  // namespace FirmwareIdentity
