# Web resource limits

DeviceFramework's built-in web interface is designed to remain responsive when a small board is busy. It bounds streamed HTTP work and WebSerial clients before the framework starts, protecting ordinary browser use within the device's measured memory headroom.

Most sketches should keep the platform-aware defaults. Change a limit only
after measuring that specific device under simultaneous browser and WebSerial
use. Configure a copy before `DeviceFramework::setup()` when it has a measured
reason to differ:

```cpp
#include <DeviceFramework.h>

void setup() {
    auto web = DeviceFrameworkWeb::defaultResourceLimits();

    // Example: this firmware needs room for a second normal status response.
    web.maxConcurrentStreamResponses = 2;

    // Keep existing diagnostic sessions working when a third browser opens.
    web.webSerialAdmissionPolicy = WebSerialAdmissionPolicy::PreserveExisting;

    DeviceFrameworkWeb::setResourceLimits(web);
    DeviceFramework::setup();
}
```

`setResourceLimits()` returns `false` for invalid limits or after web setup has started. Limits are runtime firmware policy, not persistent configuration: a local deployment profile does not alter them.

## Defaults and behaviour

| Target | Streamed responses | WebSerial clients | New-work watermark | WebSerial shedding watermark |
| --- | ---: | ---: | --- | --- |
| ESP8266 | 1 | 2 | 12 KiB free / 8 KiB largest block | 8 KiB / 6 KiB |
| ESP32 | 6 | 12 | 64 KiB free / 32 KiB largest block | 32 KiB / 16 KiB |

The largest-block check matters on ESP8266: a board may report enough total free heap but still lack one contiguous allocation for an async response. At the new-work watermark, the framework rejects a low-priority streamed response rather than allocating into a fragmented heap. At the lower shedding watermark, it closes at most one WebSerial connection so the device can recover. Normal HTTP pages, MQTT, Wi-Fi, and the control API are not evicted.

The normal new-work watermark applies to streamed HTTP responses. A WebSerial upgrade is evaluated only after ESPAsyncWebServer has constructed its socket and made its transient handshake allocation, so it uses the lower critical watermark instead; otherwise that allocation could make a healthy connection reject itself. Client capacity still applies, and an established WebSerial client is shed at the critical floor.

`WebSerialAdmissionPolicy` controls only a new WebSerial upgrade when the configured client limit is already occupied:

- `PreserveExisting` is the default. Reject the new diagnostic session.
- `ReplaceSlowest` replaces a client only when it already has queued output.
- `ExplicitTakeover` replaces the oldest session only for a client that explicitly asks for `?takeover=1`; it avoids surprise eviction during normal browser reconnects.

When a new WebSerial client is refused for capacity or memory pressure, the connection receives WebSocket close code `1013` ("try again later"). It is not reported as an authentication failure.

WebSerial carries logs, so its queue drops log data rather than repeatedly
closing and reconnecting a client when the queue is full. Keeping a live device
responsive is more important than retaining a complete debug transcript under
pressure.

## Observability

The counters are available to a sketch for its own diagnostic surface; they are not a versioned extension to the built-in status JSON:

```cpp
const DeviceFrameworkWebResourceStats web = DeviceFrameworkWeb::getResourceStats();
Serial.printf("web: streams=%u ws=%u stream-rejected=%lu (memory=%lu capacity=%lu) ws-rejected=%lu evicted=%lu dropped=%lu\n",
              web.activeStreamResponses,
              web.activeWebSerialClients,
              static_cast<unsigned long>(web.rejectedStreamResponses),
              static_cast<unsigned long>(web.rejectedStreamResponsesForMemory),
              static_cast<unsigned long>(web.rejectedStreamResponsesForCapacity),
              static_cast<unsigned long>(web.rejectedWebSerialClients),
              static_cast<unsigned long>(web.evictedWebSerialClients),
              static_cast<unsigned long>(web.droppedWebSerialBytes));
```

rejectedStreamResponses remains the aggregate for lightweight existing diagnostics. The reason counters distinguish a deliberately conservative memory-headroom refusal from the configured concurrent-response capacity.

For a resource-heavy sketch, measure its normal heap and largest free block first, change one threshold at a time, then test with actual simultaneous browser and WebSerial clients. Do not copy ESP8266 values onto ESP32 merely for consistency; the defaults intentionally preserve ESP32's substantially larger headroom.

See [Troubleshooting](TROUBLESHOOTING.md) for symptoms and recovery steps.
