#ifndef DEVICEFRAMEWORK_PARAMETER_TYPES_H
#define DEVICEFRAMEWORK_PARAMETER_TYPES_H

#include <Arduino.h>

// Configuration sources (bitflags)
enum DeviceFrameworkParameterSource {
    SOURCE_NONE = 0,
    SOURCE_WIFI_MANAGER = 1 << 0,  // Configurable via WiFiManager captive portal
    SOURCE_HOME_ASSISTANT = 1 << 1, // Configurable via Home Assistant
    SOURCE_ALL = SOURCE_WIFI_MANAGER | SOURCE_HOME_ASSISTANT  // All sources
};

// Tracks where a parameter update originated so sync behavior can avoid
// bouncing Home Assistant writes back through MQTT from inside the callback.
enum class DeviceFrameworkParameterUpdateOrigin {
    DEVICE,
    WIFI_MANAGER,
    HOME_ASSISTANT,
    HTTP_API
};

// Home Assistant device types for configuration
// Based on ArduinoHA device types suitable for runtime configuration
enum class HAConfigDeviceType {
    NONE,       // Not exposed to HA
    NUMBER,     // HANumber - numeric input with min/max/step constraints
    SWITCH,     // HASwitch - boolean on/off toggle
    SELECT,     // HASelect - dropdown with predefined options
    TEXT        // HAText - text input (if available in ArduinoHA)
};

// Home Assistant-specific constraints (for HANumber and HASelect)
struct HAConstraints {
    // For HANumber
    float minValue;
    float maxValue;
    float step;
    uint8_t precision;  // Decimal places for display
    int8_t numberMode;  // HANumber mode: -1=Auto (default), 1=Box, 2=Slider

    // For HASelect
    String options;  // Semicolon-separated list of options

    // For HAText
    uint16_t textMinLength;  // 0 means unset
    uint16_t textMaxLength;  // 0 means unset

    HAConstraints()
        : minValue(0), maxValue(100), step(1), precision(2), numberMode(-1),
          options(""), textMinLength(0), textMaxLength(0) {}
};

// HTML input attributes for WiFiManager customization
struct HTMLInputAttributes {
    String inputType;           // "text", "password", "email", "url", "number", "select"
    String autocapitalize;      // "off", "none", "on", "sentences", "words", "characters"
    String autocorrect;         // "off", "on"
    String autocomplete;        // "off", "username", "email", "url", etc.
    String inputmode;           // "text", "email", "url", "numeric", "decimal", "tel"
    String placeholder;         // Placeholder text (optional, label is used by default)
    String options;             // Options for select elements (semicolon-separated)

    HTMLInputAttributes()
        : inputType("text"),
          autocapitalize(""),
          autocorrect(""),
          autocomplete(""),
          inputmode(""),
          placeholder(""),
          options("") {}
};

// Forward declarations
class WiFiManagerParameter;

// Return type structs for parameter lists
struct ParameterIdList {
    String* ids;
    size_t count;

    ParameterIdList() : ids(nullptr), count(0) {}
    ParameterIdList(String* ids, size_t count) : ids(ids), count(count) {}

    ~ParameterIdList() {
        delete[] ids;
    }

    ParameterIdList(const ParameterIdList&) = delete;
    ParameterIdList& operator=(const ParameterIdList&) = delete;

    ParameterIdList(ParameterIdList&& other) noexcept
        : ids(other.ids), count(other.count) {
        other.ids = nullptr;
        other.count = 0;
    }

    ParameterIdList& operator=(ParameterIdList&& other) noexcept {
        if (this != &other) {
            delete[] ids;
            ids = other.ids;
            count = other.count;
            other.ids = nullptr;
            other.count = 0;
        }
        return *this;
    }
};

struct WiFiManagerParameterList {
    WiFiManagerParameter* const* parameters;
    size_t count;

    WiFiManagerParameterList() : parameters(nullptr), count(0) {}
    WiFiManagerParameterList(WiFiManagerParameter* const* parameters, size_t count)
        : parameters(parameters), count(count) {}
};

// Helper structure to track WiFiManager parameter instances
struct WiFiManagerParameterRef {
    WiFiManagerParameter* parameter;
    String parameterId;      // Persistent storage for ID (WiFiManager stores pointer, not copy!)
    String label;            // Persistent storage for label (WiFiManager stores pointer, not copy!)
    String customHTML;       // Persistent storage for custom HTML (WiFiManager stores pointer, not copy!)

    WiFiManagerParameterRef() : parameter(nullptr) {}
};

// Helper structure to track Home Assistant device instances
struct HADeviceRef {
    HAConfigDeviceType deviceType;
    void* device;  // Pointer to HANumber*, HASwitch*, HASelect*, or HAText*
    String parameterId;      // Persistent storage for ID (HA devices store pointer, not copy!)
    String defaultEntityId;  // Persistent storage for default_entity_id (ArduinoHA stores pointer, not copy!)

    HADeviceRef() : deviceType(HAConfigDeviceType::NONE), device(nullptr) {}
};

#endif // DEVICEFRAMEWORK_PARAMETER_TYPES_H
