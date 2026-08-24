#include "DeviceFrameworkRtcBlob.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../DeviceFrameworkDebug.h"
#include "../Utils/CRC32Utils.h"
#include <DeviceFrameworkPlatform.h>
#include <string.h>

namespace {

constexpr uint32_t BLOB_MAGIC = 0x44465242u;  // 'DFRB'
constexpr uint16_t BLOB_FORMAT_VERSION = 1;

#pragma pack(push, 1)
struct RtcBlobRecord {
    uint32_t magic;
    uint32_t nameHash;
    uint16_t payloadLen;
    uint16_t formatVersion;
    uint32_t payloadCrc32;
};
#pragma pack(pop)

static_assert(sizeof(RtcBlobRecord) == 16, "RtcBlobRecord size");

// Padded slot size in bytes (multiple of 4) for ESP8266 RTC and ESP32 NVS value.
static constexpr size_t SLOT_BYTES =
    (sizeof(RtcBlobRecord) + DeviceFrameworkRtcBlob::MAX_PAYLOAD + 3u) & ~3u;

static uint32_t hashName(const char* name) {
    uint32_t hash = 5381u;
    int c;
    while (name && (c = static_cast<unsigned char>(*name++)) != 0) {
        hash = ((hash << 5) + hash) + static_cast<uint32_t>(c);
    }
    return hash;
}

#ifdef DF_PLATFORM_ESP8266

extern "C" {
#include "user_interface.h"
}

// RTC user memory on ESP8266: 512 bytes (128 x 32-bit words). Framework uses CONFIG_rtcMemAddr for RtcData.
static constexpr uint16_t RTC_USER_WORDS = 128;

static uint16_t blobSlotWordOffset() {
    const uint32_t frameworkWords = (sizeof(RtcData) + 3u) / 4u;
    return static_cast<uint16_t>(CONFIG_rtcMemAddr + frameworkWords);
}

static bool readSlotBytes(uint8_t* out, size_t len) {
    if ((len & 3u) != 0u) {
        return false;
    }
    const uint16_t wordOff = blobSlotWordOffset();
    const uint16_t wordLen = static_cast<uint16_t>(len / 4u);
    if (static_cast<uint32_t>(wordOff) + wordLen > RTC_USER_WORDS || wordOff > 127) {
        LOG_ERRORLN(F("DeviceFrameworkRtcBlob: RTC slot exceeds user RTC memory"));
        return false;
    }
    if (!system_rtc_mem_read(static_cast<uint8_t>(wordOff), out, static_cast<uint16_t>(len))) {
        return false;
    }
    return true;
}

static bool writeSlotBytes(const uint8_t* data, size_t len) {
    if ((len & 3u) != 0u) {
        return false;
    }
    const uint16_t wordOff = blobSlotWordOffset();
    const uint16_t wordLen = static_cast<uint16_t>(len / 4u);
    if (static_cast<uint32_t>(wordOff) + wordLen > RTC_USER_WORDS || wordOff > 127) {
        LOG_ERRORLN(F("DeviceFrameworkRtcBlob: RTC slot exceeds user RTC memory"));
        return false;
    }
    if (!system_rtc_mem_write(static_cast<uint8_t>(wordOff), data, static_cast<uint16_t>(len))) {
        return false;
    }
    return true;
}

#endif

#ifdef DF_PLATFORM_ESP32
#include <Preferences.h>

static const char* PREFERENCES_NAMESPACE = "DeviceFramework";

static void preferencesKeyForName(const char* name, char* keyBuf, size_t keyBufLen) {
    const uint32_t h = hashName(name);
    snprintf(keyBuf, keyBufLen, "r%08lx", static_cast<unsigned long>(h));
}

#endif

}  // namespace

bool DeviceFrameworkRtcBlob::write(const char* name, const void* data, size_t len) {
    if (name == nullptr || name[0] == '\0' || data == nullptr || len == 0 || len > MAX_PAYLOAD) {
        return false;
    }

    const uint32_t nh = hashName(name);
    RtcBlobRecord header = {};
    header.magic = BLOB_MAGIC;
    header.nameHash = nh;
    header.payloadLen = static_cast<uint16_t>(len);
    header.formatVersion = BLOB_FORMAT_VERSION;
    header.payloadCrc32 = CRC32Utils::calculate(static_cast<const uint8_t*>(data), len);

    uint8_t slot[SLOT_BYTES];
    memset(slot, 0, sizeof(slot));
    memcpy(slot, &header, sizeof(header));
    memcpy(slot + sizeof(RtcBlobRecord), data, len);

    const size_t used = sizeof(RtcBlobRecord) + len;
    const size_t padded = (used + 3u) & ~3u;

#ifdef DF_PLATFORM_ESP8266
    return writeSlotBytes(slot, padded);
#elif defined(DF_PLATFORM_ESP32)
    char key[16];
    preferencesKeyForName(name, key, sizeof(key));
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
        LOG_ERRORLN(F("DeviceFrameworkRtcBlob::write: Preferences open failed"));
        return false;
    }
    const size_t written = preferences.putBytes(key, slot, padded);
    preferences.end();
    return written == padded;
#else
    (void)slot;
    (void)padded;
    return false;
#endif
}

bool DeviceFrameworkRtcBlob::read(const char* name, void* data, size_t capacity, size_t* outActualLen) {
    if (name == nullptr || name[0] == '\0' || data == nullptr || capacity == 0) {
        return false;
    }

    uint8_t slot[SLOT_BYTES];
    memset(slot, 0, sizeof(slot));
    size_t bytesRead = 0;

#ifdef DF_PLATFORM_ESP8266
    if (!readSlotBytes(slot, SLOT_BYTES)) {
        return false;
    }
    bytesRead = SLOT_BYTES;
#elif defined(DF_PLATFORM_ESP32)
    char key[16];
    preferencesKeyForName(name, key, sizeof(key));
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, true)) {
        return false;
    }
    bytesRead = preferences.getBytes(key, slot, sizeof(slot));
    preferences.end();
    if (bytesRead < sizeof(RtcBlobRecord)) {
        return false;
    }
#else
    return false;
#endif

    RtcBlobRecord header;
    memcpy(&header, slot, sizeof(header));
    if (header.magic != BLOB_MAGIC || header.formatVersion != BLOB_FORMAT_VERSION) {
        return false;
    }
    if (header.nameHash != hashName(name)) {
        return false;
    }
    if (header.payloadLen == 0 || header.payloadLen > MAX_PAYLOAD) {
        return false;
    }

    const size_t recordBytes = (sizeof(RtcBlobRecord) + header.payloadLen + 3u) & ~3u;
    if (bytesRead < recordBytes) {
        return false;
    }

    const uint8_t* payload = slot + sizeof(RtcBlobRecord);
    const uint32_t crc = CRC32Utils::calculate(payload, header.payloadLen);
    if (crc != header.payloadCrc32) {
        LOG_WARNLN(F("DeviceFrameworkRtcBlob::read: CRC mismatch"));
        return false;
    }

    if (outActualLen == nullptr) {
        if (header.payloadLen != capacity) {
            return false;
        }
    } else {
        if (header.payloadLen > capacity) {
            return false;
        }
        *outActualLen = header.payloadLen;
    }

    memcpy(data, payload, header.payloadLen);
    return true;
}

bool DeviceFrameworkRtcBlob::clear(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }

#ifdef DF_PLATFORM_ESP8266
    uint8_t zeros[SLOT_BYTES];
    memset(zeros, 0, sizeof(zeros));
    return writeSlotBytes(zeros, sizeof(zeros));
#elif defined(DF_PLATFORM_ESP32)
    char key[16];
    preferencesKeyForName(name, key, sizeof(key));
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
        return false;
    }
    const bool ok = preferences.remove(key);
    preferences.end();
    return ok;
#else
    return false;
#endif
}
