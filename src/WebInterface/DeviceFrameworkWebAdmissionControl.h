#ifndef DEVICEFRAMEWORK_WEB_ADMISSION_CONTROL_H
#define DEVICEFRAMEWORK_WEB_ADMISSION_CONTROL_H

#ifdef ENABLE_WEB_INTERFACE

#include <Arduino.h>
#include <DeviceFrameworkPlatform.h>
#include "DeviceFrameworkWebResourceLimits.h"

enum class WebSerialAdmissionResult : uint8_t {
    Accepted,
    RejectedCapacity,
    RejectedMemory,
    ReplacedSlowest,
    ReplacedForTakeover,
};

struct WebStreamPermit {
    bool active;
    uint16_t generation;
};

class DeviceFrameworkWebAdmissionControl {
public:
    static constexpr uint8_t kMaximumStreamPermits =
#if defined(DF_PLATFORM_ESP8266)
        2;
#else
        8;
#endif
#if defined(DF_PLATFORM_ESP8266)
    static constexpr uint8_t kMaximumTrackedWebSerialClients = 4;
#else
    static constexpr uint8_t kMaximumTrackedWebSerialClients = 16;
#endif

    static void begin(const DeviceFrameworkWebResourceLimits& limits);
    static void end();

    static WebStreamPermit* tryAcquireStreamPermit();
    static void releaseStreamPermit(WebStreamPermit* permit, uint16_t generation);
    static bool canStartDiagnosticWork();
    static bool isCriticalMemoryPressure();
    static WebSerialAdmissionResult admitWebSerial(uint32_t clientId,
                                                    bool explicitTakeover,
                                                    uint32_t* evictedClientId);
    static void releaseWebSerial(uint32_t clientId);
    static void updateWebSerialClient(uint32_t clientId, bool queueIsFull,
                                      bool canSend, size_t queueLen);
    static uint8_t activeWebSerialClients();
    static bool shouldShedWebSerialClient(uint32_t* evictedClientId);

    static void recordDroppedWebSerialBytes(size_t bytes);
    // Return a snapshot: WebSocket callbacks can update these counters from
    // the asynchronous networking context while a sketch reads them.
    static DeviceFrameworkWebResourceStats stats();

private:
    struct WebSerialClientRecord {
        uint32_t clientId;
        uint32_t connectedAt;
        size_t queueLen;
        bool active;
        bool queueIsFull;
        bool canSend;
    };

    static DeviceFrameworkWebResourceLimits resourceLimits;
    static DeviceFrameworkWebResourceStats resourceStats;
    static WebStreamPermit streamPermits[kMaximumStreamPermits];
    static WebSerialClientRecord webSerialClients[kMaximumTrackedWebSerialClients];

    static WebSerialClientRecord* findClient(uint32_t clientId);
    static WebSerialClientRecord* findFreeClient();
    static WebSerialClientRecord* selectSlowestClient();
    static WebSerialClientRecord* selectOldestClient();
    static bool hasMemoryHeadroom(uint32_t freeHeapThreshold,
                                  uint32_t largestBlockThreshold);
};

#endif // ENABLE_WEB_INTERFACE
#endif // DEVICEFRAMEWORK_WEB_ADMISSION_CONTROL_H
