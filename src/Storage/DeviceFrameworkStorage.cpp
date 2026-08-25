#include "DeviceFrameworkStorage.h"

#include "../Configuration/DeviceFrameworkIdentity.h"
#include "../Configuration/DeviceFrameworkParameterRegistry.h"
#include "../Configuration/DeviceFrameworkParameters.h"
#include "../DeviceFrameworkDebug.h"
#include "../Utils/CRC32Utils.h"

#include <vector>

namespace {
constexpr uint32_t kMagic = 0x44464332UL;  // DFC2
constexpr uint8_t kStateValid = 0xA5;
constexpr uint16_t kHeaderWithoutCrcSize = 36;
constexpr uint16_t kHeaderSize = 40;
constexpr uint16_t kLegacyEepromSize = 512;
constexpr char kLegacyVersion[] = "V1.0";

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

bool encodePayload(const DeviceFrameworkParameterRegistry& registry, std::vector<uint8_t>& payload) {
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

bool decodePayload(const std::vector<uint8_t>& payload, std::map<String, String>& values) {
    size_t offset = 0;
    while (offset < payload.size()) {
        const uint8_t idLength = payload[offset++];
        if (idLength == 0 || offset + idLength + 2 > payload.size()) return false;
        String id;
        for (uint8_t i = 0; i < idLength; ++i) id += static_cast<char>(payload[offset++]);
        const uint16_t valueLength = readU16(&payload[offset]);
        offset += 2;
        if (offset + valueLength > payload.size()) return false;
        String value;
        for (uint16_t i = 0; i < valueLength; ++i) value += static_cast<char>(payload[offset++]);
        values[id] = value;
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

bool readSlot(uint8_t slot, std::map<String, String>& values, uint16_t& schema, uint32_t& generation, DeviceFrameworkProvisioningState* provisioning = nullptr) {
    StorageHeader header{};
    const uint16_t base = slotBase(slot);
    if (!readHeader(base, header)) return false;
    if (header.applicationHash != applicationHash(DeviceFrameworkIdentity::getApplication().applicationId)) return false;
    std::vector<uint8_t> payload(header.payloadLength);
    for (uint16_t i = 0; i < header.payloadLength; ++i) payload[i] = EEPROM.read(base + kHeaderSize + i);
    if (CRC32Utils::calculate(payload.data(), payload.size()) != header.payloadCrc) return false;
    if (!decodePayload(payload, values)) return false;
    schema = header.schema;
    generation = header.generation;
    if (provisioning) {
        provisioning->profileHash = header.profileHash;
        provisioning->attemptedRevision = header.attemptedRevision;
        provisioning->appliedRevision = header.appliedRevision;
    }
    return true;
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
void DeviceFrameworkStorage::setup() {
    EEPROM.begin(getConfigEEPROMSize());
}

bool DeviceFrameworkStorage::save() {
    auto& registry = DeviceFrameworkParameters::getRegistry();
    std::vector<uint8_t> payload;
    if (!encodePayload(registry, payload)) {
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
    uint16_t firstSchema = 0;
    uint16_t secondSchema = 0;
    uint32_t firstGeneration = 0;
    uint32_t secondGeneration = 0;
    const bool firstValid = readSlot(0, firstValues, firstSchema, firstGeneration);
    const bool secondValid = readSlot(1, secondValues, secondSchema, secondGeneration);
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
    uint16_t verifiedSchema = 0;
    uint32_t verifiedGeneration = 0;
    if (!readSlot(target, verifiedValues, verifiedSchema, verifiedGeneration)) {
        LOG_ERRORLN(F("DeviceFramework storage: post-write verification failed"));
        return false;
    }

    lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Loaded, false);
    return true;
}

bool DeviceFrameworkStorage::readV2(std::map<String, String>& values, uint16_t& schema, uint32_t& generation) {
    std::map<String, String> firstValues;
    std::map<String, String> secondValues;
    uint16_t firstSchema = 0;
    uint16_t secondSchema = 0;
    uint32_t firstGeneration = 0;
    uint32_t secondGeneration = 0;
    DeviceFrameworkProvisioningState firstProvisioning;
    DeviceFrameworkProvisioningState secondProvisioning;
    const bool firstValid = readSlot(0, firstValues, firstSchema, firstGeneration, &firstProvisioning);
    const bool secondValid = readSlot(1, secondValues, secondSchema, secondGeneration, &secondProvisioning);
    if (!firstValid && !secondValid) return false;
    if (secondValid && (!firstValid || secondGeneration > firstGeneration)) {
        values = secondValues;
        schema = secondSchema;
        generation = secondGeneration;
        provisioningState = secondProvisioning;
    } else {
        values = firstValues;
        schema = firstSchema;
        generation = firstGeneration;
        provisioningState = firstProvisioning;
    }
    return true;
}

bool DeviceFrameworkStorage::readLegacyV1(std::map<String, String>& values) {
    const uint16_t configuredSize = getConfigEEPROMSize();
    const uint16_t candidates[] = {
        getConfigEEPROMStart(),
        configuredSize >= kLegacyEepromSize
            ? static_cast<uint16_t>(getConfigEEPROMStart() + configuredSize - kLegacyEepromSize)
            : getConfigEEPROMStart()
    };
    for (uint8_t candidateIndex = 0; candidateIndex < 2; ++candidateIndex) {
        const uint16_t base = candidates[candidateIndex];
        if (base + sizeof(kLegacyVersion) >= EEPROM.length()) continue;
        bool versionMatches = true;
        for (size_t i = 0; i < sizeof(kLegacyVersion); ++i) {
            if (EEPROM.read(base + i) != static_cast<uint8_t>(kLegacyVersion[i])) {
                versionMatches = false;
                break;
            }
        }
        if (!versionMatches) continue;

        uint16_t address = base + sizeof(kLegacyVersion);
        const uint16_t legacyCandidateEnd = static_cast<uint16_t>(base + kLegacyEepromSize);
        const uint16_t legacyEnd = legacyCandidateEnd < EEPROM.length() ? legacyCandidateEnd : EEPROM.length();
        auto& registry = DeviceFrameworkParameters::getRegistry();
        const auto ids = registry.getParameterIdsSorted();
        for (size_t i = 0; i < ids.count; ++i) {
            const DeviceFrameworkParameterMetadata* metadata = registry.getMetadata(ids.ids[i]);
            if (!metadata || address >= legacyEnd) return false;
            String value;
            bool terminated = false;
            for (uint16_t length = 0; address < legacyEnd && length <= metadata->maxLength; ++length) {
                const uint8_t byte = EEPROM.read(address++);
                if (byte == 0 || byte == 0xff) {
                    terminated = true;
                    break;
                }
                value += static_cast<char>(byte);
            }
            if (!terminated) return false;
            values[ids.ids[i]] = value;
        }
        return true;
    }
    return false;
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
    uint16_t schema = 0;
    uint32_t generation = 0;
    if (readV2(values, schema, generation)) {
        const auto& application = DeviceFrameworkIdentity::getApplication();
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

    if (readLegacyV1(values)) {
        applyValues(values);
        lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::LegacyImported, true);
        return lastLoadResult;
    }

    lastLoadResult = DeviceFrameworkStorageLoadResult(
        hasAnyStoredData() ? DeviceFrameworkStorageLoadStatus::Corrupt : DeviceFrameworkStorageLoadStatus::Empty,
        !hasAnyStoredData()
    );
    return lastLoadResult;
}

bool DeviceFrameworkStorage::reset() {
    const uint16_t end = storageEnd();
    for (uint16_t address = storageStart(); address < end; ++address) EEPROM.write(address, 0xff);
    EEPROM.commit();
    lastLoadResult = DeviceFrameworkStorageLoadResult(DeviceFrameworkStorageLoadStatus::Empty, false);
    provisioningState = DeviceFrameworkProvisioningState();
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
