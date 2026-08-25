#ifndef DEVICEFRAMEWORK_PARAMETER_REGISTRY_H
#define DEVICEFRAMEWORK_PARAMETER_REGISTRY_H

#include <Arduino.h>
#include <map>
#include <vector>
#include <functional>
#include <WiFiManager.h>
#include <ArduinoHA.h>
#include "DeviceFrameworkParameterTypes.h"
#include "DeviceFrameworkParameterMetadata.h"
#include "DeviceFrameworkParameterValue.h"
#include "DeviceFrameworkConfig.h"
#include "../DeviceFrameworkDebug.h"

// Forward declarations
class HAMqtt;
class WiFiManager;
class HAText;

// Combined parameter storage - metadata and value together
struct DeviceFrameworkParameterEntry {
    DeviceFrameworkParameterMetadata metadata;
    DeviceFrameworkParameterValue value;

    DeviceFrameworkParameterEntry() = default;

    DeviceFrameworkParameterEntry(const DeviceFrameworkParameterMetadata& meta)
        : metadata(meta), value(meta.defaultValue) {}
};

class DeviceFrameworkParameterRegistry {
private:
    // Core storage - combined metadata and values. Capacity grows only while
    // the application registers parameters during startup.
    DeviceFrameworkParameterEntry* parameters;
    size_t parameterCount;
    size_t parameterCapacity;

    // References to created WiFiManager parameters (fixed-size arrays)
    WiFiManagerParameterRef* wifiManagerRefs;
    size_t wifiManagerRefCount;

    // References to created HA devices (fixed-size arrays)
    HADeviceRef* haDeviceRefs;
    size_t haDeviceRefCount;

    // MQTT ready state tracking
    bool mqttReady;
    bool haResyncPending;
    size_t haResyncNextIndex;
    unsigned long lastHAResyncAt;

    // Map HA device pointers to parameter IDs (for static callbacks)
    static std::map<void*, String> haDeviceToParamId;
    static DeviceFrameworkParameterRegistry* instance;

public:
    // Public access to instance for configuration changes
    static DeviceFrameworkParameterRegistry* getInstance() { return instance; }

    // Callback when a parameter value changes
    typedef std::function<void(const String& id, const String& oldValue, const String& newValue)> ParameterChangeCallback;
    ParameterChangeCallback changeCallback;

    // Internal helpers
    bool validateValue(const DeviceFrameworkParameterMetadata& metadata, const String& value) const;
    void notifyValueChanged(const String& id, const String& oldValue, const String& newValue, DeviceFrameworkParameterUpdateOrigin origin);
    String generateCustomHTML(const HTMLInputAttributes& attrs) const;
    String generateCustomInputHTML(const DeviceFrameworkParameterMetadata& meta) const;
    String buildDefaultEntityId(const char* domain, const String& parameterId) const;
    String indexToOptionString(const String& options, int8_t index) const;
    bool isSensitiveParameter(const String& id) const;
    String valueForLog(const String& id, const String& value) const;

    // Array management helpers
    DeviceFrameworkParameterEntry* findParameter(const String& id);
    const DeviceFrameworkParameterEntry* findParameter(const String& id) const;
    bool addParameter(const DeviceFrameworkParameterMetadata& meta);
    bool ensureParameterCapacity(size_t required);

    DeviceFrameworkParameterRegistry();
    ~DeviceFrameworkParameterRegistry();

    // Static callbacks for HA devices
    static void onHANumberCommand(HANumeric value, HANumber* sender);
    static void onHASwitchCommand(bool state, HASwitch* sender);
    static void onHASelectCommand(int8_t index, HASelect* sender);
    static void onHATextCommand(const char* value, HAText* sender);

    // Parameter registration
    bool registerParameter(const DeviceFrameworkParameterMetadata& meta);
    bool hasParameter(const String& id) const;

    // Value access
    String getValue(const String& id) const;
    bool setValue(const String& id, const String& value, DeviceFrameworkParameterUpdateOrigin origin = DeviceFrameworkParameterUpdateOrigin::DEVICE);
    bool setValue(const String& id, const char* value, DeviceFrameworkParameterUpdateOrigin origin = DeviceFrameworkParameterUpdateOrigin::DEVICE);  // Explicit overload to prevent const char* -> bool conversion
    bool setValue(const String& id, int value, DeviceFrameworkParameterUpdateOrigin origin = DeviceFrameworkParameterUpdateOrigin::DEVICE);
    bool setValue(const String& id, float value, int decimalPlaces = 2, DeviceFrameworkParameterUpdateOrigin origin = DeviceFrameworkParameterUpdateOrigin::DEVICE);
    bool setValue(const String& id, bool value, DeviceFrameworkParameterUpdateOrigin origin = DeviceFrameworkParameterUpdateOrigin::DEVICE);

    // Convenience getters with type conversion
    int getValueAsInt(const String& id) const;
    float getValueAsFloat(const String& id) const;
    bool getValueAsBool(const String& id) const;
    const char* getValueAsCStr(const String& id) const;

    // Metadata access
    const DeviceFrameworkParameterMetadata* getMetadata(const String& id) const;
    ParameterIdList getParameterIds() const;
    ParameterIdList getParameterIdsSorted() const;  // Returns IDs sorted by order field
    ParameterIdList getParameterIds(DeviceFrameworkParameterSource source) const;  // Returns IDs filtered by source
    ParameterIdList getParameterIdsSorted(DeviceFrameworkParameterSource source) const;  // Returns IDs filtered by source and sorted by order field

    // Metadata modification
    bool setDefaultValue(const String& id, const String& defaultValue);

    // WiFiManager integration
    WiFiManagerParameterList createWiFiManagerParameters();
    void syncFromWiFiManager(WiFiManager::WiFiManagerRequestArgs requestArgs);
    void syncToWiFiManager(const String& id);
    WiFiManagerParameter* getWiFiManagerParameter(const String& id);

    // Home Assistant integration
    void createHADevices(HAMqtt& mqtt);
    void syncToHA(const String& id, bool forcePublish = false);
    void syncAllToHA();
    void scheduleFullHASync();
    void cancelPendingHASync();
    bool processPendingHASync(unsigned long now);
    bool hasPendingHASync() const;
    void setMqttReady(bool ready);
    void* getHADeviceForParameter(const String& id);

    // Persistence
    void loadFromStorage();
    void saveToStorage();

    // Callback registration
    void setChangeCallback(ParameterChangeCallback callback);

    // Debug
    void printRegistry() const;
};

#endif // DEVICEFRAMEWORK_PARAMETER_REGISTRY_H
