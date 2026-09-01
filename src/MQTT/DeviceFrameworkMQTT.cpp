#include "DeviceFrameworkMQTT.h"
#include "../Configuration/DeviceFrameworkIdentity.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../WiFi/DeviceFrameworkWiFi.h"
#include "../MDNS/DeviceFrameworkMDNS.h"

// MQTT and HA-related members
HADevice DeviceFrameworkMQTT::haDevice;
HAMqtt* DeviceFrameworkMQTT::mqttClient = nullptr;
WiFiClient DeviceFrameworkMQTT::espClient;

// MQTT command handlers
std::map<String, CommandHandler> DeviceFrameworkMQTT::commandHandlers;

// HAMqtt::begin() is a one-time configuration call. HAMqtt::loop() owns all
// connection attempts after that point.
bool DeviceFrameworkMQTT::mqttBegun = false;
bool DeviceFrameworkMQTT::mqttReconfigurationRequested = false;

// MQTT connection state tracking
bool DeviceFrameworkMQTT::wasConnected = false;

// User callback storage
MqttConnectedCallback DeviceFrameworkMQTT::userConnectedCallback = nullptr;
MqttDisconnectedCallback DeviceFrameworkMQTT::userDisconnectedCallback = nullptr;

String DeviceFrameworkMQTT::generateDeviceSpecificTopic(const HABaseDeviceType* device, const char* suffix, size_t bufferSize) {
    if (!device) {
        LOG_ERRORLN(F("Device is null in generateSharedTopic."));
        return ""; // Return an empty string
    }

    if (!device->uniqueId()) {
        LOG_ERRORLN(F("Device name is null in generateSharedTopic."));
        return ""; // Return an empty string
    }

    const char* uniqueId = haDevice.getUniqueId();
    if (!uniqueId) {
        LOG_ERRORLN(F("Device unique ID is null in generateSharedTopic."));
        return ""; // Return an empty string
    }

    // Allocate the buffer
    char topic[bufferSize];
    int result = snprintf(topic, sizeof(topic), "%s/%s/%s/%s",
                          mqttClient->getDataPrefix(),
                          uniqueId,
                          device->uniqueId(),
                          suffix ? suffix : "");

    // Check for truncation or other errors
    if (result < 0 || (size_t)result >= bufferSize) {
        LOG_ERRORLN(F("Buffer size insufficient in generateSharedTopic."));
        return ""; // Return an empty string on error
    }

    return String(topic);
}

String DeviceFrameworkMQTT::generateSharedTopic(const char* suffix, size_t bufferSize) {
    const char* uniqueId = haDevice.getUniqueId();
    if (!uniqueId) {
        LOG_ERRORLN(F("Device unique ID is null in generateDeviceSpecificTopic."));
        return ""; // Return an empty string
    }

    // Allocate the buffer
    char topic[bufferSize];
    int result = snprintf(topic, sizeof(topic), "aha/%s/%s",
                          uniqueId,
                          suffix ? suffix : "");

    // Check for truncation or other errors
    if (result < 0 || (size_t)result >= bufferSize) {
        LOG_ERRORLN(F("Buffer size insufficient in generateDeviceSpecificTopic."));
        return ""; // Return an empty string on error
    }

    return String(topic);
}

void DeviceFrameworkMQTT::addResetCommand(const char* suffix) {
    if (!suffix) {
        LOG_ERRORLN(F("Failed to add MQTT reset command: Suffix is null."));
        return;
    }

    auto resetHandler = [](const uint8_t* payload, const uint16_t length) {
        if (!payload || length == 0) {
            LOG_WARNLN(F("Reset command received with invalid payload."));
            return;
        }

        // Convert payload to a string and check its value
        char commandBuffer[length + 1]; // Create a buffer with space for null terminator
        memcpy(commandBuffer, payload, length); // Copy the payload into the buffer
        commandBuffer[length] = '\0'; // Null-terminate the buffer
        String command(commandBuffer); // Use the null-terminated buffer to create the String
        if (command == "1") {
            LOG_INFOLN(F("Valid reset command received. Acknowledging..."));

            // Acknowledge reset command by publishing "0"
            String ackTopic = generateSharedTopic("reset_command");
            if (!ackTopic.isEmpty()) {
                DeviceFrameworkMQTT::mqttClient->publish(ackTopic.c_str(), "0", false);
            } else {
                LOG_ERRORLN(F("Failed to publish reset acknowledgment: Invalid topic."));
            }

            // Perform reset operations
            LOG_INFOLN(F("Resetting Wi-Fi and device configuration..."));
            DeviceFrameworkWiFi::clearProfiles();
            // Note: EEPROM clearing and parameter restoration would be handled by the main framework
            // This is a simplified version for the module

            LOG_INFOLN(F("Restarting device..."));
            ESP.restart();
        } else {
            LOG_DEBUG_SP(F("Invalid reset command payload: "), true);
            LOG_DEBUGLN_SP(command, false);
        }
    };

    // Use registerDeviceCommandHandler to manage the handler and subscription
    registerSharedCommandHandler(suffix, resetHandler);
}

void DeviceFrameworkMQTT::addRestartCommand(const char* suffix) {
    if (!suffix) {
        LOG_ERRORLN(F("Failed to add MQTT restart command: Suffix is null."));
        return;
    }

    auto restartHandler = [](const uint8_t* payload, const uint16_t length) {
        if (!payload || length == 0) {
            LOG_WARNLN(F("Restart command received with invalid payload."));
            return;
        }

        char commandBuffer[length + 1];
        memcpy(commandBuffer, payload, length);
        commandBuffer[length] = '\0';
        String command(commandBuffer);

        if (command == "1") {
            LOG_INFOLN(F("Valid restart command received. Restarting device..."));
            ESP.restart();
        } else {
            LOG_DEBUG_SP(F("Invalid restart command payload: "), true);
            LOG_DEBUGLN_SP(command, false);
        }
    };

    registerSharedCommandHandler(suffix, restartHandler);
}

void DeviceFrameworkMQTT::setup() {
    // Setup HAMqtt client
    if (!mqttClient) {
        mqttClient = new HAMqtt(espClient, haDevice);
        mqttClient->enableDeviceDiscovery();
    }

    mqttClient->onConnected([]() {
        LOG_INFOLN(F("=== MQTT CONNECTED ==="));
        if (mqttClient) {
            LOG_INFOLN(
                String(F("DF mqtt: pubsub=")) + String(mqttClient->getPubSubState()) +
                    F(" ahaState=") + String(static_cast<int>(mqttClient->getState())) +
                    F(" wifi=") + String(WiFi.status())
            );
        }
        LOG_DEBUGLN(F("Setting device availability to ONLINE"));
        LOG_DEBUG_SP(F("Availability topic: "), true);
        LOG_DEBUGLN_SP(String(haDevice.getAvailabilityTopic()), false);
        haDevice.setAvailability(true);
        LOG_INFOLN(F("Device availability set to ONLINE"));

        // Notify parameter registry that MQTT is ready for HA device syncing
        LOG_DEBUGLN(F("onConnected callback: calling setMqttReady(true)"));
        DeviceFrameworkParameters::getRegistry().setMqttReady(true);

        // Iterate through all registered command handlers and subscribe to their topics
        for (const auto& handlerPair : commandHandlers) {
            const String& topic = handlerPair.first;

            if (HAMqtt::instance()->subscribe(topic.c_str())) {
                LOG_DEBUG_SP(F("Subscribed to topic: "), true);
                LOG_DEBUGLN_SP(topic, false);
            } else {
                LOG_ERROR_SP(F("Failed to subscribe to topic: "), true);
                LOG_DEBUGLN_SP(topic, false);
            }
        }

        // Call user callback if registered
        if (userConnectedCallback != nullptr) {
            LOG_DEBUGLN(F("Calling user onConnected callback"));
            userConnectedCallback();
        }
    });

    mqttClient->onDisconnected([]() {
        LOG_WARNLN(F("=== MQTT DISCONNECTED ==="));
        if (mqttClient) {
            LOG_WARNLN(
                String(F("DF mqtt: pubsub=")) + String(mqttClient->getPubSubState()) +
                    F(" ahaState=") + String(static_cast<int>(mqttClient->getState())) +
                    F(" wifi=") + String(WiFi.status()) +
                    F(" deferred=") + String(mqttClient->getDeferredQueueCount()) +
                    F(" reason=") + HAMqtt::diagnosticDisconnectReasonText(mqttClient->getLastDisconnectReason())
            );
            LOG_DEBUGLN(
                String(F("DF mqtt timing stamps (millis): lastRx=")) + String(mqttClient->getLastMessageAt()) +
                    F(" lastPub=") + String(mqttClient->getLastPublishAt()) +
                    F(" lastLoopOk=") + String(mqttClient->getLastLoopOkAt()) +
                    F(" lastDisc=") + String(mqttClient->getLastDisconnectAt())
            );
        }
        LOG_DEBUGLN(F("Setting device availability to OFFLINE"));
        LOG_DEBUG_SP(F("Availability topic: "), true);
        LOG_DEBUGLN_SP(String(haDevice.getAvailabilityTopic()), false);
        haDevice.setAvailability(false);
        LOG_WARNLN(F("Device availability set to OFFLINE"));

        // Notify parameter registry that MQTT is not ready for HA device syncing
        LOG_DEBUGLN(F("onDisconnected callback: calling setMqttReady(false)"));
        DeviceFrameworkParameters::getRegistry().setMqttReady(false);

        // Call user callback if registered
        if (userDisconnectedCallback != nullptr) {
            LOG_DEBUGLN(F("Calling user onDisconnected callback"));
            userDisconnectedCallback();
        }
    });

    mqttClient->onMessage(onMqttMessageHandler);
    LOG_INFOLN(F("HAMqtt client configured."));

    // Configure HADevice
    byte mac[6];
    WiFi.macAddress(mac);
    haDevice.setUniqueId(mac, sizeof(mac));
    haDevice.setName(DeviceFrameworkParameters::getDeviceName());
    haDevice.setSoftwareVersion(DeviceFrameworkIdentity::getApplication().firmwareVersion.c_str());
    haDevice.enableExtendedUniqueIds();

     // Enable shared availability and MQTT LWT
    haDevice.enableSharedAvailability(); // Shared availability for all entities
    haDevice.enableLastWill();           // Enable LWT to mark as offline if disconnected
    LOG_DEBUG_SP(F("Enabled device shared availability: "), true);
    LOG_DEBUGLN_SP(haDevice.getAvailabilityTopic(), false);
    LOG_INFOLN(F("HADevice configured."));

    // Create Home Assistant devices from parameter registry
    DeviceFrameworkParameters::getRegistry().createHADevices(*mqttClient);
}

bool DeviceFrameworkMQTT::beginMqtt() {
    if (mqttBegun) {
        return true;
    }

    if (!DeviceFrameworkWiFi::hasUsableConnection()) {
        return false;
    }

    const char* configuredServer = DeviceFrameworkParameters::getMqttServer();
    if (!configuredServer || configuredServer[0] == '\0') {
        return false;
    }

    String mqttServer(configuredServer);
    if (mqttServer.startsWith("http://")) {
        mqttServer.remove(0, 7);
    }

    IPAddress brokerIP;
    if (!DeviceFrameworkMDNS::resolveCached(mqttServer.c_str(), brokerIP)) {
        return false;
    }

    const uint32_t configuredReconnectInterval = getConfigMQTTReconnectRateLimit();
    if (configuredReconnectInterval > 0) {
        const uint16_t hamqttReconnectInterval = configuredReconnectInterval > 65535UL
            ? 65535U
            : static_cast<uint16_t>(configuredReconnectInterval);
        mqttClient->setReconnectInterval(hamqttReconnectInterval);
    }

    LOG_INFOLN(
        String(F("DF mqtt initialize server=")) + brokerIP.toString() +
            F(" port=") + String(DeviceFrameworkParameters::getMqttPort())
    );

    if (!mqttClient->begin(brokerIP, DeviceFrameworkParameters::getMqttPort(),
                           DeviceFrameworkParameters::getMqttUser(),
                           DeviceFrameworkParameters::getMqttPass())) {
        LOG_ERRORLN(F("MQTT initialization failed; HAMqtt begin was rejected."));
        return false;
    }

    mqttBegun = true;
    return true;
}

void DeviceFrameworkMQTT::loop() {
    if (!mqttClient) {
        return;
    }

    if (mqttReconfigurationRequested) {
        mqttReconfigurationRequested = false;
        if (mqttBegun) {
            LOG_INFOLN(F("MQTT broker configuration changed; reconnecting."));
            mqttClient->disconnect();
        }
        mqttBegun = false;
        wasConnected = false;
        return;
    }

    // Defer the one-time setup until WiFi and broker resolution are ready.
    if (!beginMqtt()) {
        return;
    }

    const unsigned long now = millis();

    // Process MQTT client tasks
    mqttClient->loop();

    // Check if MQTT connection state changed
    bool currentlyConnected = mqttClient->isConnected();
    if (currentlyConnected && !wasConnected) {
        // MQTT just became connected
        LOG_INFOLN(F("MQTT connection established"));
        if (mqttClient) {
            LOG_INFOLN(
                String(F("DF mqtt loop: pubsub=")) + String(mqttClient->getPubSubState()) +
                    F(" ahaState=") + String(static_cast<int>(mqttClient->getState()))
            );
        }
        wasConnected = true;
    } else if (!currentlyConnected && wasConnected) {
        // MQTT just disconnected
        LOG_WARNLN(F("MQTT connection lost"));
        if (mqttClient) {
            LOG_WARNLN(
                String(F("DF mqtt loop: pubsub=")) + String(mqttClient->getPubSubState()) +
                    F(" ahaState=") + String(static_cast<int>(mqttClient->getState())) +
                    F(" wifi=") + String(WiFi.status()) +
                    F(" deferred=") + String(mqttClient->getDeferredQueueCount()) +
                    F(" reason=") + HAMqtt::diagnosticDisconnectReasonText(mqttClient->getLastDisconnectReason())
            );
        }
        wasConnected = false;
    }

    // HAMqtt::loop() handles reconnects after beginMqtt() has configured it.
    if (!currentlyConnected) {
        return;
    }

    DeviceFrameworkParameterRegistry& registry = DeviceFrameworkParameters::getRegistry();
    if (registry.hasPendingHASync()) {
        registry.processPendingHASync(now);
    }
}

void DeviceFrameworkMQTT::requestMqttReconfiguration() {
    mqttReconfigurationRequested = true;
}

void DeviceFrameworkMQTT::onMqttMessageHandler(const char* topic, const uint8_t* payload, const uint16_t length) {
    String topicStr(topic);
    LOG_DEBUG_SP(F("Received MQTT message: "), true);
    LOG_DEBUGLN_SP(topic, false);

    auto it = commandHandlers.find(topicStr);
    if (it != commandHandlers.end()) {
        CommandHandler handler = it->second;
        if (handler) {
            handler(payload, length); // Pass only payload and length
        }
    }
    // Note: Don't warn about unhandled topics - many are handled by ArduinoHA internally
    // Only our custom command handlers are in the commandHandlers map
}

void DeviceFrameworkMQTT::registerDeviceCommandHandler(const HABaseDeviceType* device, const char* suffix, CommandHandler handler) {
    // Generate the device-specific topic
    String topic = generateDeviceSpecificTopic(device, suffix);

    // Check for invalid inputs or topic generation errors
    if (!handler) {
        LOG_ERRORLN(F("Failed to register device-specific command handler: Handler is null."));
        return;
    }
    if (topic.isEmpty()) {
        LOG_ERRORLN(F("Failed to register device-specific command handler: Generated topic is empty or invalid."));
        return;
    }

    // Check if the topic is already registered
    if (commandHandlers.find(topic) != commandHandlers.end()) {
        LOG_DEBUG_SP(F("Device-specific command handler already registered for topic: "), true);
        LOG_DEBUGLN_SP(topic, false);
        return;
    }

    // Register the handler
    commandHandlers[topic] = handler;

    LOG_DEBUG_SP(F("Registered device-specific command handler for topic: "), true);
    LOG_DEBUGLN_SP(topic, false);
}

void DeviceFrameworkMQTT::registerSharedCommandHandler(const char* suffix, CommandHandler handler) {
    // Generate the shared topic
    String topic = generateSharedTopic(suffix);

    // Check for invalid inputs or topic generation errors
    if (!handler) {
        LOG_ERRORLN(F("Failed to register shared command handler: Handler is null."));
        return;
    }
    if (topic.isEmpty()) {
        LOG_ERRORLN(F("Failed to register shared command handler: Generated topic is empty or invalid."));
        return;
    }

    // Check if the topic is already registered
    if (commandHandlers.find(topic) != commandHandlers.end()) {
        LOG_DEBUG_SP(F("Shared command handler already registered for topic: "), true);
        LOG_DEBUGLN_SP(topic, false);
        return;
    }

    // Register the handler
    commandHandlers[topic] = handler;

    LOG_DEBUG_SP(F("Registered shared command handler for topic: "), true);
    LOG_DEBUGLN_SP(topic, false);
}

HADevice& DeviceFrameworkMQTT::getHADevice() {
    return haDevice;
}

HAMqtt& DeviceFrameworkMQTT::getHAMqtt() {
    return *mqttClient;
}

bool DeviceFrameworkMQTT::isConnected() {
    return mqttClient && mqttClient->isConnected();
}

// Callback registration methods
void DeviceFrameworkMQTT::onConnected(MqttConnectedCallback callback) {
    userConnectedCallback = callback;
}

void DeviceFrameworkMQTT::onDisconnected(MqttDisconnectedCallback callback) {
    userDisconnectedCallback = callback;
}
