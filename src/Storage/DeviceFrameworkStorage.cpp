#include "DeviceFrameworkStorage.h"

#include "../Configuration/DeviceFrameworkIdentity.h"
#include "../Configuration/DeviceFrameworkParameterRegistry.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../DeviceFrameworkDebug.h"
#include "../Utils/CRC32Utils.h"

#include <vector>

namespace {
constexpr uint32_t kMagic = 0x44464334UL;  // DFC4
constexpr uint32_t kLegacyV3Magic = 0x44464333UL;  // DFC3
constexpr uint32_t kLegacyV2Magic = 0x44464332UL;  // DFC2
constexpr uint8_t kStateValid = 0xA5;
constexpr uint8_t kPayloadVersion = 1;
constexpr uint16_t kHeaderWithoutCrcSize = 36;
constexpr uint16_t kHeaderSize = 40;

struct StorageHeader {
    uint32_t applicationHash;
    uint16_t schema;
    uint32_t generation;
    uint16_t payloadLength;
    uint32_t payloadCrc;
    uint32_t profileHash;
    uint32_t attemptedRevision;
    uint32_t appliedRevision;
};

uint16_t storageStart() {
    return getConfigEEPROMStart();
}

uint16_t storageEnd() {
    const uint16_t configured = getConfigEEPROMSize();
    const uint16_t available = EEPROM.length();
    return configured < available ? configured : available;
}

uint16_t slotSize() {
    const uint16_t start = storageStart();
    const uint16_t end = storageEnd();
    return end > start ? (end - start) / 2 : 0;
}

uint16_t slotBase(uint8_t slot) {
    return storageStart() + slotSize() * slot;
}

uint32_t applicationHash(const String& value) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < value.length(); ++i) {
        hash ^= static_cast<uint8_t>(value[i]);
        hash *= 16777619UL;
    }
    return hash;
}

void appendU16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xff));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

uint16_t readU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

void appendU32(std::vector<uint8_t>& data, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) data.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xff));
}

uint32_t readU32(const uint8_t* data) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < 4; ++i) value |= static_cast<uint32_t>(data[i]) << (i * 8);
    return value;
}

bool appendParameterEntries(const DeviceFrameworkParameterRegistry& registry, std::vector<uint8_t>& payload) {
    const auto ids = registry.getParameterIds();
    for (size_t i = 0; i < ids.count; ++i) {
        const String& id = ids.ids[i];
        const String value = registry.getValue(id);
        if (id.length() == 0 || id.length() > UINT8_MAX || value.length() > UINT16_MAX) return false;
        payload.push_back(static_cast<uint8_t>(id.length()));
        payload.insert(payload.end(), id.c_str(), id.c_str() + id.length());
        appendU16(payload, static_cast<uint16_t>(value.length()));
        payload.insert(payload.end(), value.c_str(), value.c_str() + value.length());
    }
    return true;
}

bool appendStationProfiles(const WiFiManagerStationProfiles& profiles, std::vector<uint8_t>& payload) {
    if (profiles.preferredSlot >= WM_STATION_PROFILE_COUNT ||
        (profiles.lastSuccessfulSlot != WM_NO_STATION_PROFILE &&
         profiles.lastSuccessfulSlot >= WM_STATION_PROFILE_COUNT)) return false;
    if (profiles.lastSuccessfulSlot != WM_NO_STATION_PROFILE &&
        (!profiles.slots[profiles.lastSuccessfulSlot].enabled ||
         profiles.slots[profiles.lastSuccessfulSlot].ssid[0] == 0)) return false;

    for (uint8_t slot = 0; slot < WM_STATION_PROFILE_COUNT; ++slot) {
        const WiFiManagerStationProfile& profile = profiles.slots[slot];
        const bool enabled = profile.enabled && profile.ssid[0] != 0;
        if (profile.enabled && !enabled) return false;
        if (memchr(profile.ssid, 0, sizeof(profile.ssid)) == nullptr ||
            memchr(profile.password, 0, sizeof(profile.password)) == nullptr) return false;
        const size_t ssidLength = enabled ? strlen(profile.ssid) : 0;
        const size_t passwordLength = enabled && profile.hasPassword ? strlen(profile.password) : 0;
        if (ssidLength > 32 || passwordLength > 64) return false;
        const uint8_t flags = static_cast<uint8_t>((enabled ? 0x01 : 0x00) |
                                                   (enabled && profile.hasPassword ? 0x02 : 0x00));
        payload.push_back(flags);
        payload.push_back(static_cast<uint8_t>(ssidLength));
        payload.insert(payload.end(), profile.ssid, profile.ssid + ssidLength);
        payload.push_back(static_cast<uint8_t>(passwordLength));
        payload.insert(payload.end(), profile.password, profile.password + passwordLength);
    }
    payload.push_back(profiles.preferredSlot);
    payload.push_back(profiles.lastSuccessfulSlot);
    return true;
}

bool decodeStationProfiles(const uint8_t* data, size_t size, size_t& offset,
                           WiFiManagerStationProfiles& profiles) {
    profiles = WiFiManagerStationProfiles();
    for (uint8_t slot = 0; slot < WM_STATION_PROFILE_COUNT; ++slot) {
        if (offset + 3 > size) return false;
        const uint8_t flags = data[offset++];
        if ((flags & ~0x03) != 0) return false;
        const uint8_t ssidLength = data[offset++];
        if (ssidLength > 32 || offset + ssidLength + 1 > size) return false;
        WiFiManagerStationProfile& profile = profiles.slots[slot];
        if (ssidLength > 0 && memchr(data + offset, 0, ssidLength) != nullptr) return false;
        memcpy(profile.ssid, data + offset, ssidLength);
        offset += ssidLength;
        const uint8_t passwordLength = data[offset++];
        if (passwordLength > 64 || offset + passwordLength > size) return false;
        if (passwordLength > 0 && memchr(data + offset, 0, passwordLength) != nullptr) return false;
        memcpy(profile.password, data + offset, passwordLength);
        offset += passwordLength;
        profile.enabled = (flags & 0x01) != 0;
        profile.hasPassword = (flags & 0x02) != 0;
        if ((!profile.enabled && (ssidLength != 0 || passwordLength != 0 || profile.hasPassword)) ||
            (profile.enabled && ssidLength == 0) || (!profile.hasPassword && passwordLength != 0)) return false;
    }
    if (offset + 2 > size) return false;
    profiles.preferredSlot = data[offset++];
    profiles.lastSuccessfulSlot = data[offset++];
    return profiles.preferredSlot < WM_STATION_PROFILE_COUNT &&
           (profiles.lastSuccessfulSlot == WM_NO_STATION_PROFILE ||
            (profiles.lastSuccessfulSlot < WM_STATION_PROFILE_COUNT &&
             profiles.slots[profiles.lastSuccessfulSlot].enabled));
}

bool encodePayload(const DeviceFrameworkParameterRegistry& registry, const char* password,
                   const WiFiManagerStationProfiles& profiles,
                   std::vector<uint8_t>& payload) {
    if (!isConfigDevicePasswordValid(password)) return false;
    const char* value = password ? password : "";
    const size_t passwordLength = strlen(value);
    payload.push_back(kPayloadVersion);
    payload.push_back(static_cast<uint8_t>(passwordLength));
    payload.insert(payload.end(), value, value + passwordLength);
    if (!appendStationProfiles(profiles, payload)) return false;
    return appendParameterEntries(registry, payload);
}

bool decodeParameterEntries(const uint8_t* data, size_t size, std::map<String, String>& values) {
    size_t offset = 0;
    while (offset < size) {
        const uint8_t idLength = data[offset++];
        if (idLength == 0 || offset + idLength + 2 > size) return false;
        String id;
        for (uint8_t i = 0; i < idLength; ++i) id += static_cast<char>(data[offset++]);
        const uint16_t valueLength = readU16(data + offset);
        offset += 2;
        if (offset + valueLength > size) return false;
        String value;
        for (uint16_t i = 0; i < valueLength; ++i) value += static_cast<char>(data[offset++]);
        values[id] = value;
    }
    return true;
}

bool decodePayload(const std::vector<uint8_t>& payload, String& password,
                   WiFiManagerStationProfiles& profiles,
                   std::map<String, String>& values) {
    if (payload.size() < 2 || payload[0] != kPayloadVersion) return false;
    size_t offset = 1;
    const uint8_t passwordLength = payload[offset++];
    if (passwordLength >= sizeof(CONFIG_devicePassword) ||
        payload.size() < offset + static_cast<size_t>(passwordLength)) return false;
    password = "";
    for (uint8_t i = 0; i < passwordLength; ++i) password += static_cast<char>(payload[offset + i]);
    offset += passwordLength;
    if (!isConfigDevicePasswordValid(password.c_str())) return false;
    if (!decodeStationProfiles(payload.data(), payload.size(), offset, profiles)) return false;
    return decodeParameterEntries(payload.data() + offset, payload.size() - offset, values);
}

bool stationProfilesEqual(const WiFiManagerStationProfiles& first,
                         const WiFiManagerStationProfiles& second) {
    if (first.preferredSlot != second.preferredSlot ||
        first.lastSuccessfulSlot != second.lastSuccessfulSlot) return false;
    for (uint8_t slot = 0; slot < WM_STATION_PROFILE_COUNT; ++slot) {
        const WiFiManagerStationProfile& a = first.slots[slot];
        const WiFiManagerStationProfile& b = second.slots[slot];
        if (a.enabled != b.enabled || a.hasPassword != b.hasPassword) return false;
        if (a.enabled && strcmp(a.ssid, b.ssid) != 0) return false;
        if (a.hasPassword && strcmp(a.password, b.password) != 0) return false;
    }
    return true;
}

bool readHeader(uint16_t base, StorageHeader& header) {
    if (base + kHeaderSize > storageEnd()) return false;
    uint8_t raw[kHeaderSize];
    for (uint16_t i = 0; i < kHeaderSize; ++i) raw[i] = EEPROM.read(base + i);
    if (readU32(raw) != kMagic || readU16(raw + 4) != DeviceFrameworkStorage::STORAGE_FORMAT_VERSION || raw[6] != kStateValid) return false;
    if (CRC32Utils::calculate(raw, kHeaderWithoutCrcSize) != readU32(raw + kHeaderWithoutCrcSize)) return false;
    header.applicationHash = readU32(raw + 8);
    header.schema = readU16(raw + 12);
    header.generation = readU32(raw + 14);
    header.payloadLength = readU16(raw + 18);
    header.payloadCrc = readU32(raw + 20);
    header.profileHash = readU32(raw + 24);
    header.attemptedRevision = readU32(raw + 28);
    header.appliedRevision = readU32(raw + 32);
    const uint16_t available = slotSize();
    return available >= kHeaderSize && header.payloadLength <= available - kHeaderSize;
}

void writeHeader(uint16_t base, const StorageHeader& header) {
    uint8_t raw[kHeaderSize] = {};
    std::vector<uint8_t> encoded;
    appendU32(encoded, kMagic);
    appendU16(encoded, DeviceFrameworkStorage::STORAGE_FORMAT_VERSION);
    encoded.push_back(kStateValid);
    encoded.push_back(0);
    appendU32(encoded, header.applicationHash);
    appendU16(encoded, header.schema);
    appendU32(encoded, header.generation);
    appendU16(encoded, header.payloadLength);
    appendU32(encoded, header.payloadCrc);
    appendU32(encoded, header.profileHash);
    appendU32(encoded, header.attemptedRevision);
    appendU32(encoded, header.appliedRevision);
    for (uint16_t i = 0; i < kHeaderWithoutCrcSize; ++i) raw[i] = encoded[i];
    const uint32_t crc = CRC32Utils::calculate(raw, kHeaderWithoutCrcSize);
    for (uint8_t i = 0; i < 4; ++i) raw[kHeaderWithoutCrcSize + i] = static_cast<uint8_t>((crc >> (i * 8)) & 0xff);
    for (uint16_t i = 0; i < kHeaderSize; ++i) EEPROM.write(base + i, raw[i]);
}

bool readSlot(uint8_t slot, std::map<String, String>& values, String& password,
              WiFiManagerStationProfiles& profiles,
              uint16_t& schema, uint32_t& generation,
              DeviceFrameworkProvisioningState* provisioning = nullptr) {
    StorageHeader header{};
    const uint16_t base = slotBase(slot);
    if (!readHeader(base, header)) return false;
    if (header.applicationHash != applicationHash(DeviceFrameworkIdentity::getApplication().applicationId)) return false;
    std::vector<uint8_t> payload(header.payloadLength);
    for (uint16_t i = 0; i < header.payloadLength; ++i) payload[i] = EEPROM.read(base + kHeaderSize + i);
    if (CRC32Utils::calculate(payload.data(), payload.size()) != header.payloadCrc) return false;
    if (!decodePayload(payload, password, profiles, values)) return false;
    schema = header.schema;
    generation = header.generation;
    if (provisioning) {
        provisioning->profileHash = header.profileHash;
        provisioning->attemptedRevision = header.attemptedRevision;
        provisioning->appliedRevision = header.appliedRevision;
    }
    return true;
}

bool hasForeignApplicationData() {
    const uint32_t expectedApplicationHash = applicationHash(DeviceFrameworkIdentity::getApplication().applicationId);
    for (uint8_t slot = 0; slot < 2; ++slot) {
        StorageHeader header{};
        const uint16_t base = slotBase(slot);
        if (!readHeader(base, header) || header.applicationHash == expectedApplicationHash) continue;

        std::vector<uint8_t> payload(header.payloadLength);
        for (uint16_t index = 0; index < header.payloadLength; ++index) {
            payload[index] = EEPROM.read(base + kHeaderSize + index);
        }
        if (CRC32Utils::calculate(payload.data(), payload.size()) != header.payloadCrc) continue;

        std::map<String, String> values;
        String password;
        WiFiManagerStationProfiles profiles;
        if (decodePayload(payload, password, profiles, values)) return true;
    }
    return false;
}

bool hasUnsupportedLegacyData() {
    for (uint8_t slot = 0; slot < 2; ++slot) {
        const uint16_t base = slotBase(slot);
        if (base + sizeof(uint32_t) > storageEnd()) continue;
        uint8_t raw[sizeof(uint32_t)];
        for (uint8_t index = 0; index < sizeof(raw); ++index) raw[index] = EEPROM.read(base + index);
        const uint32_t magic = readU32(raw);
        if (magic == kLegacyV3Magic || magic == kLegacyV2Magic) return true;
    }
    return false;
}

bool hasAnyStoredData() {
    for (uint8_t slot = 0; slot < 2; ++slot) {
        if (EEPROM.read(slotBase(slot)) != 0xff) return true;
    }
    return false;
}
}  // namespace

DeviceFrameworkStorageLoadResult DeviceFrameworkStorage::lastLoadResult;

DeviceFrameworkProvisioningState DeviceFrameworkStorage::provisioningState;
WiFiManagerStationProfiles DeviceFrameworkStorage::stationProfiles;
void DeviceFrameworkStorage::setup() {
    EEPROM.begin(getConfigEEPROMSize());
}

bool DeviceFrameworkStorage::save() {
    return saveWithDevicePassword(getConfigDevicePassword());
}

bool DeviceFrameworkStorage::saveWithDevicePassword(const char* password) {
    auto& registry = DeviceFrameworkParameters::getRegistry();
    std::vector<uint8_t> payload;
    if (!encodePayload(registry, password, stationProfiles, payload)) {
        LOG_ERRORLN(F("DeviceFramework storage: unable to encode parameter payload"));
        return false;
    }

    const uint16_t available = slotSize();
    if (available <= kHeaderSize || payload.size() > static_cast<size_t>(available - kHeaderSize)) {
        LOG_ERRORLN(F("DeviceFramework storage: configuration exceeds transactional slot capacity"));
        return false;
    }

    std::map<String, String> firstValues;
    std::map<String, String> secondValues;
    String firstPassword;
    String secondPassword;
    WiFiManagerStationProfiles firstProfiles;
    WiFiManagerStationProfiles secondProfiles;
    uint16_t firstSchema = 0;
    uint16_t secondSchema = 0;
    uint32_t firstGeneration = 0;
    uint32_t secondGeneration = 0;
    const bool firstValid = readSlot(0, firstValues, firstPassword, firstProfiles, firstSchema, firstGeneration);
    const bool secondValid = readSlot(1, secondValues, secondPassword, secondProfiles, secondSchema, secondGeneration);
    const uint8_t target = !firstValid ? 0 : (!secondValid ? 1 : (firstGeneration <= secondGeneration ? 0 : 1));
    const uint32_t generation = max(firstGeneration, secondGeneration) + 1;
    const uint16_t base = slotBase(target);

    // Invalidate the target first; its header becomes valid only after its full payload is written.
    EEPROM.write(base + 6, 0);
    for (size_t i = 0; i < payload.size(); ++i) EEPROM.write(base + kHeaderSize + i, payload[i]);

    StorageHeader header{};
    header.applicationHash = applicationHash(DeviceFrameworkIdentity::getApplication().applicationId);
    header.schema = DeviceFrameworkIdentity::getApplication().configurationSchema;
    header.generation = generation;
    header.payloadLength = static_cast<uint16_t>(payload.size());
    header.payloadCrc = CRC32Utils::calculate(payload.data(), payload.size());
    header.profileHash = provisioningState.profileHash;
    header.attemptedRevision = provisioningState.attemptedRevision;
    header.appliedRevision = provisioningState.appliedRevision;
    writeHeader(base, header);
    EEPROM.commit();

    std::map<String, String> verifiedValues;
    String verifiedPassword;
    WiFiManagerStationProfiles verifiedProfiles;
    uint16_t verifiedSchema = 0;
    uint32_t verifiedGeneration = 0;
    if (!readSlot(target, verifiedValues, verifiedPassword, verifiedProfiles, verifiedSchema, verifiedGeneration) ||
        verifiedPassword != (password ? password : "") ||
        !stationProfilesEqual(verifiedProfiles, stationProfiles)) {
        LOG_ERRORLN(F("DeviceFramework storage: post-write verification failed"));
        return false;
    }

    lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Loaded, false);
    return true;
}

bool DeviceFrameworkStorage::saveWithStationProfiles(const WiFiManagerStationProfiles& profiles) {
    const WiFiManagerStationProfiles previous = stationProfiles;
    stationProfiles = profiles;
    if (save()) return true;
    stationProfiles = previous;
    return false;
}

const WiFiManagerStationProfiles& DeviceFrameworkStorage::getStationProfiles() {
    return stationProfiles;
}

void DeviceFrameworkStorage::setStationProfiles(const WiFiManagerStationProfiles& profiles) {
    stationProfiles = profiles;
}

bool DeviceFrameworkStorage::readCurrent(std::map<String, String>& values, String& password,
                                    uint16_t& schema, uint32_t& generation) {
    std::map<String, String> firstValues;
    std::map<String, String> secondValues;
    String firstPassword;
    String secondPassword;
    WiFiManagerStationProfiles firstProfiles;
    WiFiManagerStationProfiles secondProfiles;
    uint16_t firstSchema = 0;
    uint16_t secondSchema = 0;
    uint32_t firstGeneration = 0;
    uint32_t secondGeneration = 0;
    DeviceFrameworkProvisioningState firstProvisioning;
    DeviceFrameworkProvisioningState secondProvisioning;
    const bool firstValid = readSlot(0, firstValues, firstPassword, firstProfiles, firstSchema, firstGeneration, &firstProvisioning);
    const bool secondValid = readSlot(1, secondValues, secondPassword, secondProfiles, secondSchema, secondGeneration, &secondProvisioning);
    if (!firstValid && !secondValid) return false;
    if (secondValid && (!firstValid || secondGeneration > firstGeneration)) {
        values = secondValues;
        password = secondPassword;
        schema = secondSchema;
        generation = secondGeneration;
        provisioningState = secondProvisioning;
        stationProfiles = secondProfiles;
    } else {
        values = firstValues;
        password = firstPassword;
        schema = firstSchema;
        generation = firstGeneration;
        provisioningState = firstProvisioning;
        stationProfiles = firstProfiles;
    }
    return true;
}

bool DeviceFrameworkStorage::applyValues(const std::map<String, String>& values) {
    auto& registry = DeviceFrameworkParameters::getRegistry();
    bool allValuesValid = true;
    for (const auto& pair : values) {
        if (!registry.hasParameter(pair.first)) continue;
        if (!registry.setValue(pair.first, pair.second, DeviceFrameworkParameterUpdateOrigin::DEVICE)) {
            LOG_WARN_SP(F("DeviceFramework storage: discarded invalid parameter "), true);
            LOG_WARNLN_SP(pair.first, false);
            allValuesValid = false;
        }
    }
    return allValuesValid;
}

DeviceFrameworkStorageLoadResult DeviceFrameworkStorage::load() {
    std::map<String, String> values;
    String password;
    uint16_t schema = 0;
    uint32_t generation = 0;
    if (readCurrent(values, password, schema, generation)) {
        const auto& application = DeviceFrameworkIdentity::getApplication();
        // This record already belongs to this application. Restore its valid
        // shared password before considering schema compatibility so a
        // downgrade or migration failure does not silently open the device.
        if (!setConfigDevicePassword(password.c_str())) {
            lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Corrupt, false);
            return lastLoadResult;
        }
        if (schema > application.configurationSchema) {
            lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Incompatible, false);
            return lastLoadResult;
        }

        bool migrated = false;
        if (schema < application.configurationSchema) {
            if (!application.migration) {
                lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Incompatible, false);
                return lastLoadResult;
            }
            DeviceFrameworkConfigMigration migration(values);
            if (!application.migration(schema, migration)) {
                lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Corrupt, false);
                return lastLoadResult;
            }
            migrated = true;
        }

        const bool valuesValid = applyValues(values);
        lastLoadResult = DeviceFrameworkStorageLoadResult(
            migrated ? DeviceFrameworkStorageLoadStatus::Migrated : DeviceFrameworkStorageLoadStatus::Loaded,
            migrated || !valuesValid
        );
        return lastLoadResult;
    }

    if (hasUnsupportedLegacyData()) {
        lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::UnsupportedLegacyFormat, false);
        return lastLoadResult;
    }

    if (hasForeignApplicationData()) {
        lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::ForeignApplication, false);
        return lastLoadResult;
    }

    const bool containsStoredData = hasAnyStoredData();
    lastLoadResult = DeviceFrameworkStorageLoadResult(
        containsStoredData ? DeviceFrameworkStorageLoadStatus::Corrupt : DeviceFrameworkStorageLoadStatus::Empty,
        false
    );
    return lastLoadResult;
}

bool DeviceFrameworkStorage::reset() {
    const uint16_t end = storageEnd();
    for (uint16_t address = storageStart(); address < end; ++address) EEPROM.write(address, 0xff);
    EEPROM.commit();
    lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Empty, false);
    provisioningState = DeviceFrameworkProvisioningState();
    stationProfiles = WiFiManagerStationProfiles();
    return true;
}

const DeviceFrameworkStorageLoadResult& DeviceFrameworkStorage::getLastLoadResult() {
    return lastLoadResult;
}

const DeviceFrameworkProvisioningState& DeviceFrameworkStorage::getProvisioningState() {
    return provisioningState;
}

void DeviceFrameworkStorage::setProvisioningState(const DeviceFrameworkProvisioningState& state) {
    provisioningState = state;
}
