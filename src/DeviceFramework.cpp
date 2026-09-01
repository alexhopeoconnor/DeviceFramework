#include "DeviceFramework.h"
#include "Utils/TimeUtils.h"
#include "Utils/CRC32Utils.h"
#include "DeviceFrameworkDebug.h"
#include "Storage/DeviceFrameworkRTC.h"
#include "Logging/DeviceFrameworkWiFiManagerLogSink.h"
#include "Logging/DeviceFrameworkArduinoHALogSink.h"
#include "WiFi/DeviceFrameworkWiFi.h"
#include "Provisioning/DeviceFrameworkProvisioning.h"
#include <ArduinoHALog.h>

namespace {
DeviceFrameworkWiFiManagerLogSink g_deviceFrameworkWiFiManagerLogSink;
DeviceFrameworkArduinoHALogSink g_deviceFrameworkArduinoHALogSink;
}

#ifdef ENABLE_WEB_INTERFACE
#include "WebInterface/DeviceFrameworkWebSerial.h"
#include "WebInterface/DeviceFrameworkTemplatePlaceholders.h"
#include "WebInterface/DeviceFrameworkDeviceStatus.h"
#endif

// No static registry - it's owned by DeviceFrameworkParameters

// RTC memory management
RtcData DeviceFramework::rtcData = {};
bool DeviceFramework::rtcCleared = false;
bool DeviceFramework::beforeSetupCalled = false;

bool DeviceFramework::configureApplication(const char* applicationId, const char* firmwareVersion,
                                           uint16_t configurationSchema,
                                           DeviceFrameworkConfigMigrationCallback migration) {
    return DeviceFrameworkIdentity::configureApplication(applicationId, firmwareVersion, configurationSchema, migration);
}

const char* DeviceFramework::getLibraryVersion() {
    return DeviceFrameworkIdentity::getLibraryVersion();
}

const DeviceFrameworkApplicationIdentity& DeviceFramework::getApplicationIdentity() {
    return DeviceFrameworkIdentity::getApplication();
}
bool DeviceFramework::setUIConfig(const DeviceFrameworkUIConfig& config) {
    return DeviceFrameworkUI::setConfig(config);
}

const DeviceFrameworkUIConfig& DeviceFramework::getUIConfig() {
    return DeviceFrameworkUI::getConfig();
}

const char* DeviceFramework::getDevicePassword() {
    return getConfigDevicePassword();
}

bool DeviceFramework::setDevicePassword(const char* password) {
    if (!isConfigDevicePasswordValid(password)) return false;
    const char* value = password ? password : "";

    // Persist and verify the candidate before changing the runtime authority.
    if (!DeviceFrameworkStorage::saveWithDevicePassword(value)) return false;
    return setConfigDevicePassword(value);
}



void DeviceFramework::beforeSetup(void (*registerParametersCallback)()) {
    if (beforeSetupCalled) return;  // Already called

    // Initialize early logging with default log level
    applyDefaultLogLevel();

    // Setup storage first
    DeviceFrameworkStorage::setup();

    // Initialize DeviceFrameworkParameters BEFORE RTC check
    // This registers core parameters so triple reset can save defaults
    DeviceFrameworkParameters::initialize();

    // Call user callback to register custom parameters BEFORE RTC check
    // This ensures custom parameters are included in triple reset saves
    if (registerParametersCallback != nullptr) {
        registerParametersCallback();
    }

    // Initialize RTC memory (may call restoreDefaults/saveParameters on triple reset)
    // All parameters (core + custom) are now registered
    setupRTCMemory();

    beforeSetupCalled = true;
}

void DeviceFramework::setup() {
    // Call beforeSetup if not already called
    if (!beforeSetupCalled) {
        beforeSetup();
    }

    // Load parameters from storage (overrides defaults with saved values)
    LOG_MEMORY_BEFORE(F("loadParameters()"));
    const DeviceFrameworkStorageLoadResult storageResult = DeviceFrameworkStorage::load();
    const bool profileApplied = DeviceFrameworkProvisioning::apply(storageResult);
    if (storageResult.requiresSave || profileApplied) {
        DeviceFrameworkStorage::save();
    }
    if (DeviceFrameworkProvisioning::hasPendingWiFi()) {
        DeviceFrameworkWiFi::setProvisioningCandidate(DeviceFrameworkProvisioning::getPendingWiFiProfiles());
    }
    LOG_MEMORY_AFTER(F("loadParameters()"));

    // Setup log level system (applies saved/default level)
    LOG_MEMORY_BEFORE(F("applyLogLevel()"));
    applyLogLevel(DeviceFrameworkParameters::getLogLevel());
    LOG_MEMORY_AFTER(F("applyLogLevel()"));

    DeviceFrameworkWiFi::getWiFiManager().setLogSink(&g_deviceFrameworkWiFiManagerLogSink);
    arduinoHASetLogSink(&g_deviceFrameworkArduinoHALogSink);

    // Setup WiFi
    // WiFi accesses parameters via DeviceFrameworkParameters
    LOG_MEMORY_BEFORE(F("DeviceFrameworkWiFi::setup()"));
    DeviceFrameworkWiFi::setup();
    LOG_MEMORY_AFTER(F("DeviceFrameworkWiFi::setup()"));

    arduinoHASetNetworkStatusFn([]() -> int {
        return DeviceFrameworkWiFi::hasUsableConnection() ? WL_CONNECTED : WL_DISCONNECTED;
    });


    // Setup OTA
    LOG_MEMORY_BEFORE(F("DeviceFrameworkOTA::setup()"));
    DeviceFrameworkOTA::setup();
    LOG_MEMORY_AFTER(F("DeviceFrameworkOTA::setup()"));

    // Setup Home Assistant device and MQTT client
    LOG_MEMORY_BEFORE(F("DeviceFrameworkMQTT::setup()"));
    DeviceFrameworkMQTT::setup();
    LOG_MEMORY_AFTER(F("DeviceFrameworkMQTT::setup()"));

    // Setup web interface if enabled
#ifdef ENABLE_WEB_INTERFACE
    // DeviceFrameworkWeb::setup() handles template engine logging integration,
    // DeviceFrameworkTemplatePlaceholders::setup(), and DeviceStatusManager initialization internally
    LOG_MEMORY_BEFORE(F("DeviceFrameworkWeb::setup()"));
    DeviceFrameworkWeb::setup();
    LOG_MEMORY_AFTER(F("DeviceFrameworkWeb::setup()"));
#endif

    LOG_MEMORY_AFTER(F("DeviceFramework::setup() - Complete"));
    LOG_INFOLN(F("DeviceFramework setup completed."));
}

void DeviceFramework::loop() {
    unsigned long currentMillis = millis();

    // Clear RTC data after timeout
    if (!rtcCleared && TimeUtils::hasTimeElapsed(currentMillis, 0, CONFIG_resetTimeout)) {
        DeviceFrameworkRTC::clear();
        rtcCleared = true;
    }

    // Handle WiFiManager portal activity
    DeviceFrameworkWiFi::loop();
#ifdef ENABLE_WEB_INTERFACE
    // Complete delayed HTTP-triggered restarts even if WiFi was just reset.
    DeviceFrameworkWebHandlers::loop();
#endif


    // Network services start only after WiFiManager reports a usable address.
    if (!DeviceFrameworkWiFi::hasUsableConnection()) {
        DeviceFrameworkMDNS::onNetworkLost();
        return;
    }

    DeviceFrameworkMDNS::onNetworkReady(getSanitizedHostname());
    // Handle mDNS updates
    DeviceFrameworkMDNS::loop();

    // Handle OTA updates
    DeviceFrameworkOTA::loop();

    // Handle MQTT reconnection and processing
    DeviceFrameworkMQTT::loop();

#ifdef ENABLE_WEB_INTERFACE
    // Handle web interface loop (includes runtime status updates)
    DeviceFrameworkWeb::loop();
#endif
}

void DeviceFramework::setupRTCMemory() {
    // Initialize RTC storage if not already done
    DeviceFrameworkRTC::begin();

    // Read RTC memory
    if (!DeviceFrameworkRTC::read(&rtcData)) {
        // If read failed, initialize with zeros
        memset(&rtcData, 0, sizeof(rtcData));
    }

    // Calculate CRC of the data excluding the CRC field itself
    uint32_t crcOfData = CRC32Utils::calculate(((uint8_t*)&rtcData) + 4, sizeof(rtcData) - 4);

    if (rtcData.magic == CONFIG_rtcMagicNumber && rtcData.crc32 == crcOfData) {
        uint32_t timeSinceLastReset = TimeUtils::safeTimeDifference(millis(), rtcData.lastReset);

        if (timeSinceLastReset < CONFIG_resetTimeout) {
            // Within timeout, increment reset count
            rtcData.resetCount++;
        } else {
            // Timeout expired, reset count to 1
            rtcData.resetCount = 1;
        }

        // Always increment total reset count
        rtcData.totalResetCount++;
    } else {
        // No valid data in RTC memory; initialize it
        rtcData.magic = CONFIG_rtcMagicNumber;
        rtcData.resetCount = 1;
        rtcData.totalResetCount = 1;
    }

    // Update lastReset time
    rtcData.lastReset = millis();

    // Recalculate CRC
    rtcData.crc32 = CRC32Utils::calculate(((uint8_t*)&rtcData) + 4, sizeof(rtcData) - 4);

    // Write updated RTC data back
    DeviceFrameworkRTC::write(&rtcData);

    // Check reset count and perform actions
    if (rtcData.resetCount == 2) {
        // Double reset detected
        LOG_INFOLN(F("Double reset detected! Resetting Wi-Fi credentials."));
        DeviceFrameworkWiFi::clearProfiles();
    } else if (rtcData.resetCount >= 3) {
        // Triple reset detected
        LOG_INFOLN(F("Triple reset detected! Resetting all configurations."));
        reset(DeviceFrameworkResetScope::Factory);

        // Reset resetCount
        rtcData.resetCount = 0;

        // Update RTC memory
        rtcData.lastReset = millis();
        rtcData.crc32 = CRC32Utils::calculate(((uint8_t*)&rtcData) + 4, sizeof(rtcData) - 4);
        DeviceFrameworkRTC::write(&rtcData);

        // Restart the device
        ESP.restart();
    }
}

bool DeviceFramework::isInConfigMode() {
    return DeviceFrameworkWiFi::isInConfigMode();
}

String DeviceFramework::generateDeviceSpecificTopic(const HABaseDeviceType* device, const char* suffix, size_t bufferSize) {
    return DeviceFrameworkMQTT::generateDeviceSpecificTopic(device, suffix, bufferSize);
}

String DeviceFramework::generateSharedTopic(const char* suffix, size_t bufferSize) {
    return DeviceFrameworkMQTT::generateSharedTopic(suffix, bufferSize);
}

void DeviceFramework::addMQTTResetCommand(const char* suffix) {
    DeviceFrameworkMQTT::addResetCommand(suffix);
}

void DeviceFramework::addMQTTRestartCommand(const char* suffix) {
    DeviceFrameworkMQTT::addRestartCommand(suffix);
}

const char* DeviceFramework::getDeviceName() {
    return DeviceFrameworkParameters::getDeviceName();
}

const char* DeviceFramework::getSanitizedHostname() {
    static String sanitized;
    // Use getSanitizedHostname (not sanitize only): empty/invalid device name after wipe
    // must fall back to a non-empty host label or mDNS/OTA hostname setup fails.
    sanitized = HostnameUtils::getSanitizedHostname(getDeviceName());
    return sanitized.c_str();
}

const char* DeviceFramework::getMqttServer() {
    return DeviceFrameworkParameters::getMqttServer();
}

uint16_t DeviceFramework::getMqttPort() {
    return DeviceFrameworkParameters::getMqttPort();
}

const char* DeviceFramework::getMqttUser() {
    return DeviceFrameworkParameters::getMqttUser();
}

const char* DeviceFramework::getMqttPass() {
    return DeviceFrameworkParameters::getMqttPass();
}

void DeviceFramework::setDeviceName(const char* name) {
    DeviceFrameworkParameters::setDeviceName(name);
}

void DeviceFramework::setMqttServer(const char* server) {
    DeviceFrameworkParameters::setMqttServer(server);
}

void DeviceFramework::setMqttPort(uint16_t port) {
    DeviceFrameworkParameters::setMqttPort(port);
}

void DeviceFramework::setMqttUser(const char* user) {
    DeviceFrameworkParameters::setMqttUser(user);
}

void DeviceFramework::setMqttPass(const char* pass) {
    DeviceFrameworkParameters::setMqttPass(pass);
}

const char* DeviceFramework::getCustomParameterValue(const char* id) {
    return DeviceFrameworkParameters::getValue(id);
}

void DeviceFramework::setCustomParameterValue(const char* id, const char* value) {
    DeviceFrameworkParameters::setValue(id, value);
}

WiFiManager& DeviceFramework::getWiFiManager() {
    return DeviceFrameworkWiFi::getWiFiManager();
}

HADevice& DeviceFramework::getHADevice() {
    return DeviceFrameworkMQTT::getHADevice();
}

HAMqtt& DeviceFramework::getHAMqtt() {
    return DeviceFrameworkMQTT::getHAMqtt();
}

DeviceFrameworkParameterRegistry& DeviceFramework::getParameterRegistry() {
    return DeviceFrameworkParameters::getRegistry();
}

void DeviceFramework::registerDeviceCommandHandler(const HABaseDeviceType* device, const char* suffix, CommandHandler handler) {
    DeviceFrameworkMQTT::registerDeviceCommandHandler(device, suffix, handler);
}

void DeviceFramework::registerSharedCommandHandler(const char* suffix, CommandHandler handler) {
    DeviceFrameworkMQTT::registerSharedCommandHandler(suffix, handler);
}

void DeviceFramework::setSaveConfigCallback(void (*callback)()) {
    DeviceFrameworkWiFi::setSaveConfigCallback(callback);
}

void DeviceFramework::setConfigModeCallback(void (*callback)()) {
    DeviceFrameworkWiFi::setConfigModeCallback(callback);
}

void DeviceFramework::restoreDefaultParameters() {
    DeviceFrameworkParameters::restoreDefaults();
}

void DeviceFramework::reset(DeviceFrameworkResetScope scope) {
    if (scope == DeviceFrameworkResetScope::WiFiOnly || scope == DeviceFrameworkResetScope::Factory) {
        DeviceFrameworkWiFi::clearProfiles();
    }
    if (scope == DeviceFrameworkResetScope::ParametersOnly || scope == DeviceFrameworkResetScope::Factory) {
        const String existingPassword(getDevicePassword());
        const WiFiManagerStationProfiles existingProfiles(DeviceFrameworkStorage::getStationProfiles());
        DeviceFrameworkStorage::reset();
        restoreDefaultParameters();
        if (scope == DeviceFrameworkResetScope::ParametersOnly) {
            // A parameter reset deliberately retains network identity as well
            // as the shared device password.
            DeviceFrameworkStorage::setStationProfiles(existingProfiles);
            DeviceFrameworkStorage::saveWithDevicePassword(existingPassword.c_str());
        } else {
            // Factory reset is deliberately unseeded. On the next boot a
            // selected profile may create its initial V4 record; otherwise
            // normal provisioning determines the new local configuration.
            setConfigDevicePassword("");
        }
    }
}

void DeviceFramework::saveParameters() {
    DeviceFrameworkStorage::save();
}

void DeviceFramework::loadParameters() {
    DeviceFrameworkStorage::load();
}

#ifdef ENABLE_WEB_INTERFACE
void DeviceFramework::setupWebInterface() {
    DeviceFrameworkWeb::setup();
}

void DeviceFramework::shutdownWebInterface() {
    DeviceFrameworkWeb::shutdown();
}

void DeviceFramework::restartWebInterface() {
    DeviceFrameworkWeb::restart();
}

void DeviceFramework::webInterfaceLoop() {
    // Web interface loop handles runtime status updates and WebSerial maintenance
#ifdef ENABLE_WEB_INTERFACE
    DeviceFrameworkWeb::loop();
#endif
}

bool DeviceFramework::isWebInterfaceEnabled() {
    return DeviceFrameworkWeb::isEnabled();
}
#endif
