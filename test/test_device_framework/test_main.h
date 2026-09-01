#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include <ArduinoHA.h>
#include "utils/test_utils.h"

// Test function declarations
// Group 1: Stateless Utilities
void test_hostname_utils();
void test_crc32_utils();
void test_time_utils();

// Group 2: Initial State Verification
void test_device_framework_basic_methods();
void test_device_framework_setup_verification();
void test_eeprom_storage_methods();
void test_storage_legacy_markers_allow_profile_cutover();

// Group 3: Configuration & Storage
void test_device_framework_configuration();
void test_device_framework_ui_configuration();
void test_storage_incompatible_schema_retains_device_password();
void test_storage_save_load();
void test_storage_falls_back_to_prior_valid_slot();
void test_storage_station_profiles_round_trip();
void test_storage_foreign_application_is_distinguished();
void test_profile_password_is_persistent_without_reprovisioning();

// Group 4: Network-Dependent
void test_wifi_manager_state();
void test_device_framework_mqtt_settings();
void test_mqtt_command_handlers();

// Group 5: ParameterRegistry Integration
void test_parameter_registry_integration();
void test_parameter_registry_ha_origin_updates_shadow_state();

// Group 6: Web Interface
void test_web_interface_methods();

// Dummy test device entities (simulating typical consuming sketch)
extern HASensorNumber testSensor;
extern HASwitch testSwitch;

#endif // TEST_MAIN_H
