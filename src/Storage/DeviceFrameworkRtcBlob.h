#ifndef DEVICEFRAMEWORK_RTC_BLOB_H
#define DEVICEFRAMEWORK_RTC_BLOB_H

#include <Arduino.h>

/**
 * Small restart-resilient blob storage for sketch state.
 *
 * ESP8266: stored in RTC user memory immediately after the framework's RtcData block
 *          (see CONFIG_rtcMemAddr and RtcData in DeviceFrameworkConfig.h).
 * ESP32:   stored in NVS under the same Preferences namespace as DeviceFrameworkRTC.
 *
 * Blobs are keyed by a short ASCII name; use a stable prefix per feature
 * (e.g. "project.relay_intent").
 * Max payload size is MAX_PAYLOAD bytes.
 *
 * ESP8266: a single RTC slot is used; the most recent write wins. Different names overwrite
 * the same physical region (read fails if the stored name hash does not match).
 */
class DeviceFrameworkRtcBlob {
public:
    static constexpr size_t MAX_PAYLOAD = 64;

    /**
     * Write a named blob. Replaces any previous blob with the same name.
     * @param name non-null, short identifier (e.g. "project.relay_intent")
     * @param data payload bytes (may be a packed struct); length must be <= MAX_PAYLOAD
     */
    static bool write(const char* name, const void* data, size_t len);

    /**
     * Read a named blob into buffer.
     * If outActualLen is null, stored length must equal capacity or read fails.
     * If outActualLen is non-null, on success it receives the stored payload length.
     */
    static bool read(const char* name, void* data, size_t capacity, size_t* outActualLen = nullptr);

    /** Invalidate a named blob (slot cleared / key removed). */
    static bool clear(const char* name);
};

#endif
