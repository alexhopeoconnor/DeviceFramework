#ifdef ENABLE_WEB_INTERFACE

#include "DeviceFrameworkWebAdmissionControl.h"

#include <DeviceFrameworkPlatform.h>
#include <limits.h>

#if defined(DF_PLATFORM_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace {

#if defined(DF_PLATFORM_ESP32)
portMUX_TYPE webAdmissionLock = portMUX_INITIALIZER_UNLOCKED;

class WebAdmissionGuard {
public:
    WebAdmissionGuard() { portENTER_CRITICAL(&webAdmissionLock); }
    ~WebAdmissionGuard() { portEXIT_CRITICAL(&webAdmissionLock); }
};

#define DEVICEFRAMEWORK_WEB_ADMISSION_GUARD WebAdmissionGuard guard
#else
// ESP8266 AsyncTCP invokes this code cooperatively on its sole core, so there
// is no second task to protect against. Avoid interrupt masking in the hot
// WebSerial maintenance path; ESP32 retains the cross-task critical section.
#define DEVICEFRAMEWORK_WEB_ADMISSION_GUARD
#endif

}  // namespace
DeviceFrameworkWebResourceLimits DeviceFrameworkWebAdmissionControl::resourceLimits = {};
DeviceFrameworkWebResourceStats DeviceFrameworkWebAdmissionControl::resourceStats = {};
WebStreamPermit DeviceFrameworkWebAdmissionControl::streamPermits[
    DeviceFrameworkWebAdmissionControl::kMaximumStreamPermits] = {};
DeviceFrameworkWebAdmissionControl::WebSerialClientRecord
    DeviceFrameworkWebAdmissionControl::webSerialClients[
        DeviceFrameworkWebAdmissionControl::kMaximumTrackedWebSerialClients] = {};

void DeviceFrameworkWebAdmissionControl::begin(
    const DeviceFrameworkWebResourceLimits& limits) {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    resourceLimits = limits;
    resourceStats = {};
    for (auto& permit : streamPermits) {
        permit.active = false;
        permit.generation = 0;
    }
    for (auto& client : webSerialClients) {
        client = {};
    }
}

void DeviceFrameworkWebAdmissionControl::end() {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    resourceStats.activeStreamResponses = 0;
    resourceStats.activeWebSerialClients = 0;
    for (auto& permit : streamPermits) {
        permit.active = false;
    }
    for (auto& client : webSerialClients) {
        client = {};
    }
}

bool DeviceFrameworkWebAdmissionControl::hasMemoryHeadroom(
    uint32_t freeHeapThreshold, uint32_t largestBlockThreshold) {
    const uint32_t freeHeap = ESP.getFreeHeap();
#if defined(DF_PLATFORM_ESP8266)
    const uint32_t largestBlock = ESP.getMaxFreeBlockSize();
#else
    const uint32_t largestBlock = ESP.getMaxAllocHeap();
#endif
    return freeHeap >= freeHeapThreshold && largestBlock >= largestBlockThreshold;
}

bool DeviceFrameworkWebAdmissionControl::canStartDiagnosticWork() {
    return hasMemoryHeadroom(resourceLimits.memory.rejectNewBelowFreeHeapBytes,
                             resourceLimits.memory.rejectNewBelowLargestBlockBytes);
}

bool DeviceFrameworkWebAdmissionControl::isCriticalMemoryPressure() {
    return !hasMemoryHeadroom(resourceLimits.memory.shedWebSerialBelowFreeHeapBytes,
                              resourceLimits.memory.shedWebSerialBelowLargestBlockBytes);
}

WebStreamPermit* DeviceFrameworkWebAdmissionControl::tryAcquireStreamPermit() {
    const bool memoryHeadroom = canStartDiagnosticWork();
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    if (!memoryHeadroom ||
        resourceStats.activeStreamResponses >= resourceLimits.maxConcurrentStreamResponses) {
        ++resourceStats.rejectedStreamResponses;
        return nullptr;
    }

    for (auto& permit : streamPermits) {
        if (!permit.active) {
            permit.active = true;
            ++permit.generation;
            if (permit.generation == 0) {
                ++permit.generation;
            }
            ++resourceStats.activeStreamResponses;
            return &permit;
        }
    }

    ++resourceStats.rejectedStreamResponses;
    return nullptr;
}

void DeviceFrameworkWebAdmissionControl::releaseStreamPermit(
    WebStreamPermit* permit, uint16_t generation) {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    if (permit == nullptr || !permit->active || permit->generation != generation) {
        return;
    }

    permit->active = false;
    if (resourceStats.activeStreamResponses > 0) {
        --resourceStats.activeStreamResponses;
    }
}


DeviceFrameworkWebAdmissionControl::WebSerialClientRecord*
DeviceFrameworkWebAdmissionControl::findClient(uint32_t clientId) {
    for (auto& client : webSerialClients) {
        if (client.active && client.clientId == clientId) {
            return &client;
        }
    }
    return nullptr;
}

DeviceFrameworkWebAdmissionControl::WebSerialClientRecord*
DeviceFrameworkWebAdmissionControl::findFreeClient() {
    for (auto& client : webSerialClients) {
        if (!client.active) {
            return &client;
        }
    }
    return nullptr;
}

DeviceFrameworkWebAdmissionControl::WebSerialClientRecord*
DeviceFrameworkWebAdmissionControl::selectSlowestClient() {
    WebSerialClientRecord* selected = nullptr;
    for (auto& client : webSerialClients) {
        if (!client.active) {
            continue;
        }
        if (selected == nullptr ||
            (client.queueIsFull && !selected->queueIsFull) ||
            (client.queueIsFull == selected->queueIsFull && client.queueLen > selected->queueLen)) {
            selected = &client;
        }
    }
    return selected;
}

DeviceFrameworkWebAdmissionControl::WebSerialClientRecord*
DeviceFrameworkWebAdmissionControl::selectOldestClient() {
    WebSerialClientRecord* selected = nullptr;
    for (auto& client : webSerialClients) {
        if (client.active && (selected == nullptr || client.connectedAt < selected->connectedAt)) {
            selected = &client;
        }
    }
    return selected;
}

WebSerialAdmissionResult DeviceFrameworkWebAdmissionControl::admitWebSerial(
    uint32_t clientId, bool explicitTakeover, uint32_t* evictedClientId) {
    if (evictedClientId != nullptr) {
        *evictedClientId = 0;
    }
    const bool memoryHeadroom = canStartDiagnosticWork();
    const bool criticalPressure = isCriticalMemoryPressure();
    const uint32_t connectedAt = millis();
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    if (!memoryHeadroom || criticalPressure) {
        ++resourceStats.rejectedWebSerialClients;
        return WebSerialAdmissionResult::RejectedMemory;
    }

    if (resourceStats.activeWebSerialClients >= resourceLimits.maxWebSerialClients) {
        WebSerialClientRecord* selected = nullptr;
        WebSerialAdmissionResult result = WebSerialAdmissionResult::RejectedCapacity;

        if (resourceLimits.webSerialAdmissionPolicy == WebSerialAdmissionPolicy::ReplaceSlowest) {
            selected = selectSlowestClient();
            if (selected != nullptr && (selected->queueIsFull || selected->queueLen > 0)) {
                result = WebSerialAdmissionResult::ReplacedSlowest;
            } else {
                selected = nullptr;
            }
        } else if (resourceLimits.webSerialAdmissionPolicy == WebSerialAdmissionPolicy::ExplicitTakeover &&
                   explicitTakeover) {
            selected = selectOldestClient();
            result = WebSerialAdmissionResult::ReplacedForTakeover;
        }

        if (selected == nullptr) {
            ++resourceStats.rejectedWebSerialClients;
            return WebSerialAdmissionResult::RejectedCapacity;
        }

        if (evictedClientId != nullptr) {
            *evictedClientId = selected->clientId;
        }
        selected->active = false;
        if (resourceStats.activeWebSerialClients > 0) {
            --resourceStats.activeWebSerialClients;
        }
        ++resourceStats.evictedWebSerialClients;

        WebSerialClientRecord* record = findFreeClient();
        if (record == nullptr) {
            ++resourceStats.rejectedWebSerialClients;
            return WebSerialAdmissionResult::RejectedCapacity;
        }
        *record = {clientId, connectedAt, 0, true, false, true};
        ++resourceStats.activeWebSerialClients;
        return result;
    }

    WebSerialClientRecord* record = findFreeClient();
    if (record == nullptr) {
        ++resourceStats.rejectedWebSerialClients;
        return WebSerialAdmissionResult::RejectedCapacity;
    }

    *record = {clientId, connectedAt, 0, true, false, true};
    ++resourceStats.activeWebSerialClients;
    return WebSerialAdmissionResult::Accepted;
}

void DeviceFrameworkWebAdmissionControl::releaseWebSerial(uint32_t clientId) {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    WebSerialClientRecord* client = findClient(clientId);
    if (client == nullptr) {
        return;
    }
    client->active = false;
    if (resourceStats.activeWebSerialClients > 0) {
        --resourceStats.activeWebSerialClients;
    }
}

void DeviceFrameworkWebAdmissionControl::updateWebSerialClient(
    uint32_t clientId, bool queueIsFull, bool canSend, size_t queueLen) {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    WebSerialClientRecord* client = findClient(clientId);
    if (client == nullptr) {
        return;
    }
    client->queueIsFull = queueIsFull;
    client->canSend = canSend;
    client->queueLen = queueLen;
}

uint8_t DeviceFrameworkWebAdmissionControl::activeWebSerialClients() {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    return resourceStats.activeWebSerialClients;
}

bool DeviceFrameworkWebAdmissionControl::shouldShedWebSerialClient(uint32_t* evictedClientId) {
    if (evictedClientId != nullptr) {
        *evictedClientId = 0;
    }
    const bool criticalPressure = isCriticalMemoryPressure();
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    if (!criticalPressure) {
        return false;
    }
    WebSerialClientRecord* selected = selectSlowestClient();
    if (selected == nullptr) {
        return false;
    }
    if (evictedClientId != nullptr) {
        *evictedClientId = selected->clientId;
    }
    selected->active = false;
    if (resourceStats.activeWebSerialClients > 0) {
        --resourceStats.activeWebSerialClients;
    }
    ++resourceStats.evictedWebSerialClients;
    return true;
}

void DeviceFrameworkWebAdmissionControl::recordDroppedWebSerialBytes(size_t bytes) {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    const uint32_t remaining = UINT32_MAX - resourceStats.droppedWebSerialBytes;
    resourceStats.droppedWebSerialBytes += bytes > remaining ? remaining : static_cast<uint32_t>(bytes);
}

DeviceFrameworkWebResourceStats DeviceFrameworkWebAdmissionControl::stats() {
    DEVICEFRAMEWORK_WEB_ADMISSION_GUARD;
    return resourceStats;
}

#endif // ENABLE_WEB_INTERFACE
