#ifndef DEVICEFRAMEWORK_H
#define DEVICEFRAMEWORK_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoHA.h>
#include "MDNS/DeviceFrameworkMDNS.h"
#include "Utils/HostnameUtils.h"
#include <DeviceFrameworkPlatform.h>  // Platform abstraction layer
#include <WiFiManager.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <vector>
#include <map>

#ifdef ENABLE_WEB_INTERFACE
#include <ESPAsyncWebServer.h>
#endif

// Include our modular components
#include "Configuration/DeviceFrameworkConfig.h"
#include "Configuration/DeviceFrameworkIdentity.h"
#include "Configuration/DeviceFrameworkParameterRegistry.h"
#include "Configuration/DeviceFrameworkParameters.h"
#include "DeviceFrameworkDebug.h"
#include "WiFi/DeviceFrameworkWiFi.h"
#include "MQTT/DeviceFrameworkMQTT.h"
#include "Storage/DeviceFrameworkStorage.h"
#include "Storage/DeviceFrameworkRtcBlob.h"
#include "OTA/DeviceFrameworkOTA.h"

#ifdef ENABLE_WEB_INTERFACE
#include "WebInterface/DeviceFrameworkWeb.h"
#endif

// Define CommandHandler type for MQTT command handlers
typedef void (*CommandHandler)(const uint8_t* payload, const uint16_t length);

enum class DeviceFrameworkResetScope {
    WiFiOnly,
    ParametersOnly,
    Factory
};

class DeviceFramework {
public:
    static bool configureApplication(const char* applicationId, const char* firmwareVersion,
                                     uint16_t configurationSchema,
                                     DeviceFrameworkConfigMigrationCallback migration = nullptr);
    static const char* getLibraryVersion();
    static const DeviceFrameworkApplicationIdentity& getApplicationIdentity();
    // Optional password shared by the provisioning AP, OTA, HTTP Basic auth,
    // and WebSerial. Updates are written transactionally before becoming live;
    // restart after success to reconfigure all already-started transports.
    static const char* getDevicePassword();
    static bool setDevicePassword(const char* password);


    // Initialize core systems with optional callback for custom parameter registration
    static void beforeSetup(void (*registerParametersCallback)() = nullptr);

    // Sets up the framework
    static void setup();


    // Core loop method
    static void loop();

    // Check if the device is in config mode
    static bool isInConfigMode();

    // MQTT command topic helpers
    static String generateDeviceSpecificTopic(const HABaseDeviceType* device, const char* suffix, size_t bufferSize = 128);
    static String generateSharedTopic(const char* suffix, size_t bufferSize = 128);

    // Utility methods for common MQTT commands
    static void addMQTTResetCommand(const char* suffix = "reset_command");
    static void addMQTTRestartCommand(const char* suffix = "restart_command");

    // Access parameter values
    static const char* getDeviceName();
    static const char* getSanitizedHostname();
    static const char* getMqttServer();
    static uint16_t getMqttPort();
    static const char* getMqttUser();
    static const char* getMqttPass();

    // Setters for predefined parameters
    static void setDeviceName(const char* name);
    static void setMqttServer(const char* server);
    static void setMqttPort(uint16_t port);
    static void setMqttUser(const char* user);
    static void setMqttPass(const char* pass);

    // Get or set the value of a custom parameter by ID
    static const char* getCustomParameterValue(const char* id);
    static void setCustomParameterValue(const char* id, const char* value);

    // Core API Accessors
    static WiFiManager& getWiFiManager();
    static HADevice& getHADevice();
    static HAMqtt& getHAMqtt();
    static DeviceFrameworkParameterRegistry& getParameterRegistry();

    // MQTT command handler registration
    static void registerDeviceCommandHandler(const HABaseDeviceType* device, const char* suffix, CommandHandler handler);
    static void registerSharedCommandHandler(const char* suffix, CommandHandler handler);

    // Set user-defined callbacks
    static void setSaveConfigCallback(void (*callback)());
    static void setConfigModeCallback(void (*callback)());

    // Reset and configuration persistence
    static void reset(DeviceFrameworkResetScope scope);
    static void restoreDefaultParameters();

    // Save and load parameters from transactional storage
    static void saveParameters();
    static void loadParameters();

    // Web interface methods (only available when ENABLE_WEB_INTERFACE is defined)
#ifdef ENABLE_WEB_INTERFACE
    static void setupWebInterface();
    static void shutdownWebInterface();
    static void restartWebInterface();
    static void webInterfaceLoop();
    static bool isWebInterfaceEnabled();
#endif

private:
    // RTC memory management
    static RtcData rtcData;
    static bool rtcCleared;
    static bool beforeSetupCalled;

    // RTC Memory Management
    static void setupRTCMemory();  // Initialize RTC memory and handle reset behavior
};

#endif // DEVICEFRAMEWORK_H