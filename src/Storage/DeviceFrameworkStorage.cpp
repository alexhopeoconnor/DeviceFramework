#include "DeviceFrameworkStorage.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include "../Configuration/DeviceFrameworkParameters.h"

void DeviceFrameworkStorage::setup() {
    // Initialize EEPROM with the configured size
    // On both ESP8266 and ESP32, EEPROM.begin() can be called multiple times safely
    // ESP32: Preferences API handles multiple calls gracefully
    // ESP8266: Can be called multiple times, reinitializes if size differs
    EEPROM.begin(getConfigEEPROMSize());
}

void DeviceFrameworkStorage::save() {
    auto& registry = DeviceFrameworkParameters::getRegistry();
    uint16_t addr = getConfigEEPROMStart();

    // Write the version to EEPROM byte by byte (using strlen + 1 for proper null termination)
    const char* version = getConfigVersion();
    size_t versionLen = strlen(version) + 1;  // +1 for null terminator
    for (size_t i = 0; i < versionLen; i++) {
        EEPROM.write(addr++, version[i]);
    }

    // Get all parameter IDs in sorted order (by order field)
    auto paramIds = registry.getParameterIdsSorted();
    for (size_t i = 0; i < paramIds.count; i++) {
        const String& id = paramIds.ids[i];
        String value = registry.getValue(id);
        uint8_t length = value.length();  // Actual length of the string (excluding null terminator)
        const DeviceFrameworkParameterMetadata* metadata = registry.getMetadata(id);
        const bool sensitive = metadata && metadata->htmlAttributes.inputType.equalsIgnoreCase("password");

        LOG_DEBUG_SP(F("Saving parameter: "), true);
        LOG_DEBUG_SP(id, false);
        LOG_DEBUG_SP(F(" = '"), false);
        LOG_DEBUG_SP(sensitive ? String(F("[redacted]")) : value, false);
        LOG_DEBUG_SP(F("' (length: "), false);
        LOG_DEBUG_SP(String(length), false);
        LOG_DEBUGLN_SP(F(")"), false);

        // Write the string data
        for (uint8_t j = 0; j < length; j++) {
            EEPROM.write(addr++, value[j]);
        }

        // Write the null terminator
        EEPROM.write(addr++, '\0');
    }

    EEPROM.commit();
    LOG_INFOLN(F("Parameters have been saved to EEPROM."));
}

void DeviceFrameworkStorage::load() {
    auto& registry = DeviceFrameworkParameters::getRegistry();
    uint16_t addr = getConfigEEPROMStart();

    // Read the version from EEPROM (using strlen + 1 for proper null termination)
    const char* expectedVersion = getConfigVersion();
    size_t versionLen = strlen(expectedVersion) + 1;  // +1 for null terminator
    char storedVersion[versionLen + 1];
    memset(storedVersion, 0, sizeof(storedVersion));

    for (size_t i = 0; i < versionLen; i++) {
        storedVersion[i] = EEPROM.read(addr++);
    }

    if (strcmp(storedVersion, expectedVersion) != 0) {
        LOG_WARN_SP(F("EEPROM version mismatch or uninitialized. Expected: "), true);
        LOG_WARN_SP(String(expectedVersion), false);
        LOG_WARN_SP(F(", Found: "), false);
        LOG_WARNLN_SP(String(storedVersion), false);
        LOG_WARNLN(F("EEPROM version mismatch - clearing and restoring default parameters."));
        clear();
        save();  // Save current parameter values (should be defaults)
        return;
    }

    // Get all parameter IDs in sorted order (must match save order)
    auto paramIds = registry.getParameterIdsSorted();
    for (size_t i = 0; i < paramIds.count; i++) {
        const String& id = paramIds.ids[i];
        const DeviceFrameworkParameterMetadata* meta = registry.getMetadata(id);
        if (!meta) {
            continue; // Skip if metadata not found
        }

        uint8_t maxLength = meta->maxLength;         // Get the parameter's max length
        size_t bufferLength = maxLength + 1;           // Buffer size: maxLength data bytes + 1 null terminator
        char buffer[bufferLength];
        memset(buffer, 0, bufferLength);               // Initialize to zero (null-terminated)

        // Read up to bufferLength bytes (data + null terminator), stopping at null terminator
        // Save format: [data bytes][null terminator] = up to bufferLength bytes total
        for (size_t j = 0; j < bufferLength; j++) {
            char c = EEPROM.read(addr++);
            if (c == '\0' || (unsigned char)c == 0xFF) {  // Stop at null terminator or uninitialized data
                break;
            }
            // Store data bytes in buffer (only up to maxLength to preserve null terminator at buffer[maxLength])
            // In normal operation, we break at null terminator before j reaches maxLength
            // This check protects against corrupted EEPROM data with missing null terminators
            if (j < maxLength) {
                buffer[j] = c;
            }
        }

        // Set the parameter's value in the registry
        registry.setValue(id, String(buffer));
    }

    LOG_INFOLN(F("Parameters successfully loaded from EEPROM."));
}

void DeviceFrameworkStorage::clear() {
    for (size_t i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0xFF);  // Write 0xFF to indicate "empty" state
    }
    EEPROM.commit();
    LOG_INFOLN(F("EEPROM has been cleared."));
}

bool DeviceFrameworkStorage::isVersionValid() {
    uint16_t addr = getConfigEEPROMStart();
    const char* expectedVersion = getConfigVersion();
    size_t versionLen = strlen(expectedVersion) + 1;  // +1 for null terminator
    char storedVersion[versionLen + 1];
    memset(storedVersion, 0, sizeof(storedVersion));

    for (size_t i = 0; i < versionLen; i++) {
        storedVersion[i] = EEPROM.read(addr++);
    }

    LOG_DEBUG_SP(F("EEPROM version check - Expected: "), true);
    LOG_DEBUG_SP(String(expectedVersion), false);
    LOG_DEBUG_SP(F(", Found: "), false);
    LOG_DEBUGLN_SP(String(storedVersion), false);

    return strcmp(storedVersion, expectedVersion) == 0;
}
