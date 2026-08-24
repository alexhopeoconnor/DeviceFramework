#ifndef DEVICEFRAMEWORK_HA_METADATA_H
#define DEVICEFRAMEWORK_HA_METADATA_H

#include <Arduino.h>

/**
 * Optional Home Assistant discovery fields shared across entity types.
 * When a field is empty, DeviceFramework falls back to legacy flat fields on
 * DeviceFrameworkParameterMetadata (haIcon, haDeviceClass, etc.) where applicable.
 */
struct HAEntityCommonMetadata {
    String icon;
    String deviceClass;
    String entityCategory;
    String entityPicture;
    String encoding;
    bool enabledByDefault = true;
    bool hasEnabledByDefault = false;
    uint8_t qos = 0;
    bool hasQos = false;
};

struct HATemplateMetadata {
    String valueTemplate;
    String commandTemplate;
    String payloadReset;
    String jsonAttributesTemplate;
    String lastResetValueTemplate;
};

struct HAAvailabilityMetadata {
    String payloadAvailable;
    String payloadNotAvailable;
    String availabilityMode;
    /// Semicolon-separated list of full MQTT availability topics (optional).
    String availabilityTopics;
};

struct HASensorPresentationMetadata {
    int8_t suggestedDisplayPrecision = -1;
    String options;
};

/// Device registry / integration metadata (not per-entity MQTT component).
struct HAIntegrationDeviceMetadata {
    String modelId;
    String hardwareVersion;
    String serialNumber;
    String suggestedArea;
    String viaDevice;
    String supportUrl;
    /// Raw JSON array for MQTT `connections`, e.g. [["mac","aa:bb:cc:dd:ee:ff"]]
    String connectionsJson;
};

#endif
