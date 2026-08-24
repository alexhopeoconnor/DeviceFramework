#ifndef DEVICEFRAMEWORK_PARAMETER_VALUE_H
#define DEVICEFRAMEWORK_PARAMETER_VALUE_H

#include <Arduino.h>

// Runtime parameter value - always stored as string
// Consuming code performs conversions as needed (like converting MQTT port to int)
class DeviceFrameworkParameterValue {
private:
    String value;

public:
    DeviceFrameworkParameterValue() : value("") {}
    explicit DeviceFrameworkParameterValue(const String& val) : value(val) {}

    // Get value as string
    String asString() const {
        return value;
    }

    // Conversion helpers (consuming code can use these for convenience)
    int asInt() const {
        return value.toInt();
    }

    float asFloat() const {
        return value.toFloat();
    }

    bool asBool() const {
        return value == "1" || value.equalsIgnoreCase("true") ||
               value.equalsIgnoreCase("on") || value.equalsIgnoreCase("yes");
    }

    // Set value from different types
    void setValue(const String& val) {
        value = val;
    }

    void setValue(const char* val) {
        value = String(val);
    }

    void setValue(int val) {
        value = String(val);
    }

    void setValue(float val, int decimalPlaces = 2) {
        value = String(val, decimalPlaces);
    }

    void setValue(bool val) {
        value = val ? "true" : "false";
    }

    // Get raw C string (useful for WiFiManager)
    const char* c_str() const {
        return value.c_str();
    }

    // Check if empty
    bool isEmpty() const {
        return value.length() == 0;
    }
};

#endif // DEVICEFRAMEWORK_PARAMETER_VALUE_H
