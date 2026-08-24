#ifndef DEVICEFRAMEWORK_PARAMETER_METADATA_H
#define DEVICEFRAMEWORK_PARAMETER_METADATA_H

#include <Arduino.h>
#include "DeviceFrameworkParameterTypes.h"
#include "DeviceFrameworkHAMetadata.h"

// Parameter metadata - defines a parameter's characteristics
struct DeviceFrameworkParameterMetadata {
    String id;                              // Unique identifier (e.g., "mqtt_server")
    String label;                           // Human-readable label (e.g., "MQTT Server")
    uint8_t sources;                        // Bitfield of DeviceFrameworkParameterSource values
    String defaultValue;                    // Default value (always stored as string)
    uint8_t maxLength;                      // Maximum string length (for storage and WiFiManager)
    uint16_t order;                         // Storage order (lower values saved first)

    // Home Assistant configuration (only if SOURCE_HOME_ASSISTANT is set)
    HAConfigDeviceType haDeviceType;        // Type of HA device (NUMBER, SELECT, TEXT)
    HAConstraints haConstraints;            // HA-specific constraints (min/max/step/precision/options)
    String haUnitOfMeasurement;             // Unit for HA (e.g., "°C", "m", "%")
    String haIcon;                          // Legacy fallback icon; prefer haEntityCommon.icon
    String haDeviceClass;                   // Legacy fallback device class; prefer haEntityCommon.deviceClass

    HAEntityCommonMetadata haEntityCommon;          // Shared entity discovery metadata
    HATemplateMetadata haTemplates;                 // Optional discovery templates / payload overrides
    HAAvailabilityMetadata haAvailability;          // Availability metadata
    HASensorPresentationMetadata haSensorPresentation; // Sensor-only presentation metadata
    HAIntegrationDeviceMetadata haIntegrationDevice;   // Device/origin discovery metadata

    // HTML input attributes (only if SOURCE_WIFI_MANAGER is set)
    HTMLInputAttributes htmlAttributes;     // HTML input customization for WiFiManager portal

    DeviceFrameworkParameterMetadata()
        : sources(SOURCE_NONE),
          maxLength(32),
          order(1000),
          haDeviceType(HAConfigDeviceType::NONE) {}

    DeviceFrameworkParameterMetadata(const String& id, const String& label, uint8_t sources, const String& defaultValue, uint8_t maxLen = 32, uint16_t ord = 1000)
        : id(id), label(label), sources(sources), defaultValue(defaultValue),
          maxLength(maxLen), order(ord), haDeviceType(HAConfigDeviceType::NONE) {}

    // Check if parameter supports a specific source
    bool supportsSource(DeviceFrameworkParameterSource source) const {
        return (sources & source) != 0;
    }

    // Check if exposed to WiFiManager
    bool isWiFiManagerParameter() const {
        return supportsSource(SOURCE_WIFI_MANAGER);
    }

    // Check if exposed to Home Assistant
    bool isHomeAssistantParameter() const {
        return supportsSource(SOURCE_HOME_ASSISTANT);
    }
};

#endif // DEVICEFRAMEWORK_PARAMETER_METADATA_H
