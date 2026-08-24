#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <ArduinoHA.h>
#include <WiFiClient.h>
#include <map>
#include <Arduino.h>
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../DeviceFrameworkDebug.h"

// Forward declaration
class DeviceFrameworkWiFi;

    // Define CommandHandler type for MQTT command handlers
    typedef void (*CommandHandler)(const uint8_t* payload, const uint16_t length);

    // Define callback types for MQTT events
    typedef void (*MqttConnectedCallback)();
    typedef void (*MqttDisconnectedCallback)();

class DeviceFrameworkMQTT {
private:
    static HADevice haDevice;
    static HAMqtt* mqttClient;
    static WiFiClient espClient;
    static std::map<String, CommandHandler> commandHandlers;

    // MQTT reconnection rate limiting
    static unsigned long lastReconnectAttempt;

    // MQTT connection state tracking
    static bool wasConnected;

    // User callback storage
    static MqttConnectedCallback userConnectedCallback;
    static MqttDisconnectedCallback userDisconnectedCallback;

    // Internal methods
    static void onMqttMessageHandler(const char* topic, const uint8_t* payload, const uint16_t length);

public:
    static void setup();
    static void loop();
    static void reconnect();

    // Topic generation
    static String generateDeviceSpecificTopic(const HABaseDeviceType* device, const char* suffix, size_t bufferSize = 128);
    static String generateSharedTopic(const char* suffix, size_t bufferSize = 128);

    // Command handlers
    static void registerDeviceCommandHandler(const HABaseDeviceType* device, const char* suffix, CommandHandler handler);
    static void registerSharedCommandHandler(const char* suffix, CommandHandler handler);

    // Utility commands
    static void addResetCommand(const char* suffix = "reset_command");
    static void addRestartCommand(const char* suffix = "restart_command");

    // Callback registration
    static void onConnected(MqttConnectedCallback callback);
    static void onDisconnected(MqttDisconnectedCallback callback);

    // Accessors
    static HADevice& getHADevice();
    static HAMqtt& getHAMqtt();
    static bool isConnected();
};

#endif // MQTT_MANAGER_H
