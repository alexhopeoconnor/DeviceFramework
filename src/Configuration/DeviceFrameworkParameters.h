#ifndef DEVICEFRAMEWORK_PARAMETERS_H
#define DEVICEFRAMEWORK_PARAMETERS_H

#include <Arduino.h>
#include "DeviceFrameworkParameterRegistry.h"

// Forward declarations
enum class LogLevel;

/**
 * DeviceFrameworkParameters - Runtime parameter access API
 *
 * This class provides type-safe, convenient access to runtime configuration parameters.
 * It acts as a facade over the ParameterRegistry, hiding parameter IDs and providing
 * a clean API for parameter access.
 */
class DeviceFrameworkParameters {
private:
    static DeviceFrameworkParameterRegistry registry;

public:
    // Core parameter IDs (public for registration in WiFi module)
    // NOTE: If you add or remove core parameter IDs below, update CONFIG_minParameters in DeviceFrameworkConfig.h to match!
    static const char* PARAM_DEVICE_NAME;
    static const char* PARAM_MQTT_SERVER;
    static const char* PARAM_MQTT_PORT;
    static const char* PARAM_MQTT_USER;
    static const char* PARAM_MQTT_PASS;
    static const char* PARAM_LOG_LEVEL;

    // Initialize and register core framework parameters
    static void initialize();

    // Register core framework parameters (called during initialization)
    static void registerCoreParameters();

    // Generic parameter access by ID
    static const char* getValue(const char* id);
    static bool setValue(const char* id, const char* value);
    static bool setValue(const char* id, int value);
    static bool setValue(const char* id, float value, int decimalPlaces = 2);
    static bool setValue(const char* id, bool value);

    // Core parameter getters (type-safe)
    static const char* getDeviceName();
    static const char* getMqttServer();
    static uint16_t getMqttPort();
    static const char* getMqttUser();
    static const char* getMqttPass();
    static const char* getLogLevel();      // Returns as human-readable string ("Off", "Error", "Warn", "Info", "Verbose")
    static int getLogLevelAsInt();         // Returns as int 0-4 (enum value)

    // Core parameter setters (type-safe)
    static void setDeviceName(const char* name);
    static void setMqttServer(const char* server);
    static void setMqttPort(uint16_t port);
    static void setMqttUser(const char* user);
    static void setMqttPass(const char* pass);
    static void setLogLevel(LogLevel level);     // Accepts LogLevel enum value

    // Parameter management
    static void restoreDefaults();

    // Access to the underlying registry (available before framework initialization)
    static DeviceFrameworkParameterRegistry& getRegistry();
};

#endif // DEVICEFRAMEWORK_PARAMETERS_H
