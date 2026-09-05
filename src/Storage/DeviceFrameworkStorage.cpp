#include "DeviceFrameworkStorage.h"

#include "../Configuration/DeviceFrameworkIdentity.h"
#include "../Configuration/DeviceFrameworkParameterRegistry.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../DeviceFrameworkDebug.h"
#include "../Utils/CRC32Utils.h"


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

uint16_t readU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readU32(const uint8_t* data) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < 4; ++i) value |= static_cast<uint32_t>(data[i]) << (i * 8);
    return value;
}

void writeU16(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>(value & 0xff);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xff);
}

void writeU32(uint8_t* data, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) data[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
}

class PayloadWriter {
public:
    PayloadWriter(uint16_t base, uint16_t capacity)
        : _base(base), _capacity(capacity), _length(0), _crc(CRC32Utils::initialValue()), _valid(true) {}

    bool appendByte(uint8_t value) {
        if (!_valid || _length >= _capacity) {
            _valid = false;
            return false;
        }
        EEPROM.write(_base + _length, value);
        _crc = CRC32Utils::update(_crc, value);
        ++_length;
        return true;
    }

    bool appendBytes(const char* values, size_t length) {
        if (!values && length != 0) {
            _valid = false;
            return false;
        }
        for (size_t index = 0; index < length; ++index) {
            if (!appendByte(static_cast<uint8_t>(values[index]))) return false;
        }
        return true;
    }

    bool appendU16(uint16_t value) {
        return appendByte(static_cast<uint8_t>(value & 0xff)) &&
               appendByte(static_cast<uint8_t>((value >> 8) & 0xff));
    }

    uint16_t length() const { return _length; }
    uint32_t crc() const { return _crc; }
    bool valid() const { return _valid; }

private:
    uint16_t _base;
    uint16_t _capacity;
    uint16_t _length;
    uint32_t _crc;
    bool _valid;
};

bool appendParameterEntries(const DeviceFrameworkParameterRegistry& registry, PayloadWriter& payload) {
    const auto ids = registry.getParameterIds();
    for (size_t i = 0; i < ids.count; ++i) {
        const String& id = ids.ids[i];
        const String value = registry.getValue(id);
        if (id.length() == 0 || id.length() > UINT8_MAX || value.length() > UINT16_MAX) return false;
        if (!payload.appendByte(static_cast<uint8_t>(id.length())) ||
            !payload.appendBytes(id.c_str(), id.length()) ||
            !payload.appendU16(static_cast<uint16_t>(value.length())) ||
            !payload.appendBytes(value.c_str(), value.length())) return false;
    }
    return true;
}

bool appendStationProfiles(const WiFiManagerStationProfiles& profiles, PayloadWriter& payload) {
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
        if (!payload.appendByte(flags) || !payload.appendByte(static_cast<uint8_t>(ssidLength)) ||
            !payload.appendBytes(profile.ssid, ssidLength) ||
            !payload.appendByte(static_cast<uint8_t>(passwordLength)) ||
            !payload.appendBytes(profile.password, passwordLength)) return false;
    }
    return payload.appendByte(profiles.preferredSlot) && payload.appendByte(profiles.lastSuccessfulSlot);
}

class PayloadReader {
public:
    PayloadReader(uint16_t base, uint16_t length) : _base(base), _length(length), _offset(0) {}

    bool readByte(uint8_t& value) {
        if (_offset >= _length) return false;
        value = EEPROM.read(_base + _offset++);
        return true;
    }

    bool readBytes(char* destination, size_t length) {
        if (!destination || length > remaining()) return false;
        for (size_t index = 0; index < length; ++index) {
            destination[index] = static_cast<char>(EEPROM.read(_base + _offset++));
        }
        return true;
    }

    bool skip(size_t length) {
        if (length > remaining()) return false;
        _offset += length;
        return true;
    }

    size_t remaining() const { return _length - _offset; }

private:
    uint16_t _base;
    uint16_t _length;
    uint16_t _offset;
};

bool decodeStationProfiles(PayloadReader& reader, WiFiManagerStationProfiles& profiles) {
    profiles = WiFiManagerStationProfiles();
    for (uint8_t slot = 0; slot < WM_STATION_PROFILE_COUNT; ++slot) {
        uint8_t flags = 0;
        uint8_t ssidLength = 0;
        if (!reader.readByte(flags) || !reader.readByte(ssidLength) || (flags & ~0x03) != 0) return false;
        if (ssidLength > 32 || reader.remaining() < static_cast<size_t>(ssidLength) + 1) return false;
        WiFiManagerStationProfile& profile = profiles.slots[slot];
        if (!reader.readBytes(profile.ssid, ssidLength) ||
            (ssidLength > 0 && memchr(profile.ssid, 0, ssidLength) != nullptr)) return false;
        uint8_t passwordLength = 0;
        if (!reader.readByte(passwordLength) || passwordLength > 64 || reader.remaining() < passwordLength) return false;
        if (!reader.readBytes(profile.password, passwordLength) ||
            (passwordLength > 0 && memchr(profile.password, 0, passwordLength) != nullptr)) return false;
        profile.enabled = (flags & 0x01) != 0;
        profile.hasPassword = (flags & 0x02) != 0;
        if ((!profile.enabled && (ssidLength != 0 || passwordLength != 0 || profile.hasPassword)) ||
            (profile.enabled && ssidLength == 0) || (!profile.hasPassword && passwordLength != 0)) return false;
    }
    if (!reader.readByte(profiles.preferredSlot) || !reader.readByte(profiles.lastSuccessfulSlot)) return false;
    return profiles.preferredSlot < WM_STATION_PROFILE_COUNT &&
           (profiles.lastSuccessfulSlot == WM_NO_STATION_PROFILE ||
            (profiles.lastSuccessfulSlot < WM_STATION_PROFILE_COUNT &&
             profiles.slots[profiles.lastSuccessfulSlot].enabled));
}

bool writePayload(uint16_t base, uint16_t capacity,
                  const DeviceFrameworkParameterRegistry& registry, const char* password,
                  const WiFiManagerStationProfiles& profiles,
                  uint16_t& length, uint32_t& crc) {
    if (!isConfigDevicePasswordValid(password)) return false;
    const char* value = password ? password : "";
    const size_t passwordLength = strlen(value);
    PayloadWriter payload(base, capacity);
    if (!payload.appendByte(kPayloadVersion) ||
        !payload.appendByte(static_cast<uint8_t>(passwordLength)) ||
        !payload.appendBytes(value, passwordLength) ||
        !appendStationProfiles(profiles, payload) ||
        !appendParameterEntries(registry, payload) ||
        !payload.valid()) return false;
    length = payload.length();
    crc = payload.crc();
    return true;
}

bool decodeParameterEntries(PayloadReader& reader, std::map<String, String>& values) {
    while (reader.remaining() > 0) {
        uint8_t idLength = 0;
        if (!reader.readByte(idLength) || idLength == 0 || reader.remaining() < static_cast<size_t>(idLength) + 2) return false;
        String id;
        if (!id.reserve(idLength)) return false;
        for (uint8_t index = 0; index < idLength; ++index) {
            uint8_t byte = 0;
            if (!reader.readByte(byte) || !id.concat(static_cast<char>(byte))) return false;
        }
        uint8_t valueLow = 0;
        uint8_t valueHigh = 0;
        if (!reader.readByte(valueLow) || !reader.readByte(valueHigh)) return false;
        const uint16_t valueLength = static_cast<uint16_t>(valueLow) |
            (static_cast<uint16_t>(valueHigh) << 8);
        if (reader.remaining() < valueLength) return false;
        String value;
        if (!value.reserve(valueLength)) return false;
        for (uint16_t index = 0; index < valueLength; ++index) {
            uint8_t byte = 0;
            if (!reader.readByte(byte) || !value.concat(static_cast<char>(byte))) return false;
        }
        values[id] = value;
    }
    return true;
}

bool decodePayload(uint16_t base, uint16_t length, String& password,
                   WiFiManagerStationProfiles& profiles,
                   std::map<String, String>& values) {
    PayloadReader reader(base, length);
    uint8_t payloadVersion = 0;
    uint8_t passwordLength = 0;
    if (!reader.readByte(payloadVersion) || payloadVersion != kPayloadVersion ||
        !reader.readByte(passwordLength) || passwordLength >= sizeof(CONFIG_devicePassword) ||
        reader.remaining() < passwordLength) return false;
    password = "";
    if (!password.reserve(passwordLength)) return false;
    for (uint8_t index = 0; index < passwordLength; ++index) {
        uint8_t byte = 0;
        if (!reader.readByte(byte) || !password.concat(static_cast<char>(byte))) return false;
    }
    if (!isConfigDevicePasswordValid(password.c_str())) return false;
    if (!decodeStationProfiles(reader, profiles)) return false;
    return decodeParameterEntries(reader, values);
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
bool slotPayloadCrcMatches(uint16_t base, const StorageHeader& header) {
    uint32_t crc = CRC32Utils::initialValue();
    for (uint16_t index = 0; index < header.payloadLength; ++index) {
        crc = CRC32Utils::update(crc, EEPROM.read(base + kHeaderSize + index));
    }
    return crc == header.payloadCrc;
}

bool inspectSlot(uint8_t slot, uint32_t& generation) {
    StorageHeader header{};
    const uint16_t base = slotBase(slot);
    if (!readHeader(base, header)) return false;
    if (header.applicationHash != applicationHash(DeviceFrameworkIdentity::getApplication().applicationId)) return false;
    if (!slotPayloadCrcMatches(base, header)) return false;
    generation = header.generation;
    return true;
}

bool writtenSlotMatches(uint16_t base, const StorageHeader& expected) {
    StorageHeader actual{};
    if (!readHeader(base, actual) ||
        actual.applicationHash != expected.applicationHash || actual.schema != expected.schema ||
        actual.generation != expected.generation || actual.payloadLength != expected.payloadLength ||
        actual.payloadCrc != expected.payloadCrc || actual.profileHash != expected.profileHash ||
        actual.attemptedRevision != expected.attemptedRevision ||
        actual.appliedRevision != expected.appliedRevision) return false;
    return slotPayloadCrcMatches(base, actual);
}

void writeHeader(uint16_t base, const StorageHeader& header) {
    uint8_t raw[kHeaderSize] = {};
    writeU32(raw, kMagic);
    writeU16(raw + 4, DeviceFrameworkStorage::STORAGE_FORMAT_VERSION);
    raw[6] = kStateValid;
    writeU32(raw + 8, header.applicationHash);
    writeU16(raw + 12, header.schema);
    writeU32(raw + 14, header.generation);
    writeU16(raw + 18, header.payloadLength);
    writeU32(raw + 20, header.payloadCrc);
    writeU32(raw + 24, header.profileHash);
    writeU32(raw + 28, header.attemptedRevision);
    writeU32(raw + 32, header.appliedRevision);
    const uint32_t crc = CRC32Utils::calculate(raw, kHeaderWithoutCrcSize);
    writeU32(raw + kHeaderWithoutCrcSize, crc);
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
    if (!slotPayloadCrcMatches(base, header)) return false;
    if (!decodePayload(base + kHeaderSize, header.payloadLength, password, profiles, values)) return false;
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

        // A complete V4 header and payload CRC identify a record written by
        // another application. Its parameters are intentionally opaque here:
        // decoding them would allocate transient maps solely to classify data
        // we must not load.
        if (slotPayloadCrcMatches(base, header)) return true;
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
    const uint16_t available = slotSize();
    if (available <= kHeaderSize) {
        LOG_ERRORLN(F("DeviceFramework storage: configuration exceeds transactional slot capacity"));
        return false;
    }

    uint32_t firstGeneration = 0;
    uint32_t secondGeneration = 0;
    const bool firstValid = inspectSlot(0, firstGeneration);
    const bool secondValid = inspectSlot(1, secondGeneration);
    const uint8_t target = !firstValid ? 0 : (!secondValid ? 1 : (firstGeneration <= secondGeneration ? 0 : 1));
    const uint32_t generation = max(firstGeneration, secondGeneration) + 1;
    const uint16_t base = slotBase(target);

    // Invalidate the target first; its header becomes valid only after its full payload is written.
    EEPROM.write(base + 6, 0);
    uint16_t payloadLength = 0;
    uint32_t payloadCrc = CRC32Utils::initialValue();
    if (!writePayload(base + kHeaderSize, available - kHeaderSize,
                      registry, password, stationProfiles, payloadLength, payloadCrc)) {
        LOG_ERRORLN(F("DeviceFramework storage: configuration exceeds transactional slot capacity"));
        return false;
    }

    StorageHeader header{};
    header.applicationHash = applicationHash(DeviceFrameworkIdentity::getApplication().applicationId);
    header.schema = DeviceFrameworkIdentity::getApplication().configurationSchema;
    header.generation = generation;
    header.payloadLength = payloadLength;
    header.payloadCrc = payloadCrc;
    header.profileHash = provisioningState.profileHash;
    header.attemptedRevision = provisioningState.attemptedRevision;
    header.appliedRevision = provisioningState.appliedRevision;
    writeHeader(base, header);
    EEPROM.commit();

    if (!writtenSlotMatches(base, header)) {
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
    uint32_t firstGeneration = 0;
    uint32_t secondGeneration = 0;
    const bool firstValid = inspectSlot(0, firstGeneration);
    const bool secondValid = inspectSlot(1, secondGeneration);
    if (!firstValid && !secondValid) return false;

    const uint8_t preferredSlot = secondValid && (!firstValid || secondGeneration > firstGeneration) ? 1 : 0;
    const uint8_t fallbackSlot = preferredSlot == 0 ? 1 : 0;
    const bool fallbackIsValid = fallbackSlot == 0 ? firstValid : secondValid;
    WiFiManagerStationProfiles loadedProfiles;
    DeviceFrameworkProvisioningState loadedProvisioning;

    values.clear();
    password = "";
    schema = 0;
    generation = 0;
    bool decoded = readSlot(preferredSlot, values, password, loadedProfiles, schema, generation, &loadedProvisioning);
    if (!decoded && fallbackIsValid) {
        values.clear();
        password = "";
        loadedProfiles = WiFiManagerStationProfiles();
        loadedProvisioning = DeviceFrameworkProvisioningState();
        schema = 0;
        generation = 0;
        decoded = readSlot(fallbackSlot, values, password, loadedProfiles, schema, generation, &loadedProvisioning);
    }
    if (!decoded) return false;

    provisioningState = loadedProvisioning;
    stationProfiles = loadedProfiles;
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
