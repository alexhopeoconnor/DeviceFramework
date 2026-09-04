#ifndef DEVICEFRAMEWORK_WEB_RESOURCE_LIMITS_H
#define DEVICEFRAMEWORK_WEB_RESOURCE_LIMITS_H

#ifdef ENABLE_WEB_INTERFACE

#include <Arduino.h>

// Controls what happens when WebSerial has reached its configured client
// capacity while the device still has normal memory headroom.
enum class WebSerialAdmissionPolicy : uint8_t {
    PreserveExisting,
    ReplaceSlowest,
    ExplicitTakeover,
};

// The first pair protects normal device work by refusing new diagnostic work.
// The second pair is a lower emergency floor where WebSerial may be shed to
// keep Wi-Fi, MQTT, OTA, and the device loop responsive.
struct DeviceFrameworkWebMemoryWatermarks {
    uint32_t rejectNewBelowFreeHeapBytes;
    uint32_t rejectNewBelowLargestBlockBytes;
    uint32_t shedWebSerialBelowFreeHeapBytes;
    uint32_t shedWebSerialBelowLargestBlockBytes;
};

// A sketch configures these before DeviceFrameworkWeb::setup(). Limits bound
// concurrent work; they do not preallocate TCP clients or response buffers.
struct DeviceFrameworkWebResourceLimits {
    uint8_t maxConcurrentStreamResponses;
    uint8_t maxWebSerialClients;
    DeviceFrameworkWebMemoryWatermarks memory;
    WebSerialAdmissionPolicy webSerialAdmissionPolicy;
};

struct DeviceFrameworkWebResourceStats {
    uint8_t activeStreamResponses;
    uint8_t activeWebSerialClients;
    uint32_t rejectedStreamResponses;
    uint32_t rejectedWebSerialClients;
    uint32_t evictedWebSerialClients;
    uint32_t droppedWebSerialBytes;
};

#endif // ENABLE_WEB_INTERFACE
#endif // DEVICEFRAMEWORK_WEB_RESOURCE_LIMITS_H
