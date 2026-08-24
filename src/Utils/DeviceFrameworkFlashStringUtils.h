#ifndef DEVICEFRAMEWORK_FLASHSTRINGUTILS_H
#define DEVICEFRAMEWORK_FLASHSTRINGUTILS_H

#include <Arduino.h>
#include <IPAddress.h>

/**
 * DeviceFrameworkFlashStringUtils
 *
 * Utility class for efficiently building strings from flash strings and variables
 * using char buffers to avoid multiple String allocations and heap fragmentation.
 *
 * This utility works with char buffers and flash strings (F() macro) to build
 * messages efficiently by appending components directly to a buffer and returning
 * a pointer to the next position for continued building.
 *
 * Usage:
 *   char buffer[128];
 *   char* ptr = buffer;
 *   ptr = DeviceFrameworkFlashStringUtils::appendFlashString(ptr, F("Prefix: "));
 *   ptr = DeviceFrameworkFlashStringUtils::appendString(ptr, variable);
 *   ptr = DeviceFrameworkFlashStringUtils::appendIntWithSuffix(ptr, value, F(" units"));
 *   sendDebugToWebSocket(String(buffer));
 */
class DeviceFrameworkFlashStringUtils {
public:
    /**
     * Append a flash string to the buffer at the current position
     * @param buffer Current position in the char buffer
     * @param flashStr Flash string to append (from F() macro)
     * @return Pointer to the position after the appended string
     */
    static char* appendFlashString(char* buffer, const __FlashStringHelper* flashStr);

    /**
     * Append a regular String object to the buffer at the current position
     * @param buffer Current position in the char buffer
     * @param str String object to append
     * @return Pointer to the position after the appended string
     */
    static char* appendString(char* buffer, const String& str);

    /**
     * Append a signed integer to the buffer at the current position
     * @param buffer Current position in the char buffer
     * @param value Integer value to append
     * @return Pointer to the position after the appended integer
     */
    static char* appendInt(char* buffer, int32_t value);

    /**
     * Append an unsigned integer to the buffer at the current position
     * @param buffer Current position in the char buffer
     * @param value Unsigned integer value to append
     * @return Pointer to the position after the appended integer
     */
    static char* appendUInt(char* buffer, uint32_t value);

    /**
     * Append a signed integer with a flash string suffix to the buffer
     * @param buffer Current position in the char buffer
     * @param value Integer value to append
     * @param suffix Flash string suffix to append after the integer
     * @return Pointer to the position after the appended integer and suffix
     */
    static char* appendIntWithSuffix(char* buffer, int32_t value, const __FlashStringHelper* suffix);

    /**
     * Append an unsigned integer with a flash string suffix to the buffer
     * @param buffer Current position in the char buffer
     * @param value Unsigned integer value to append
     * @param suffix Flash string suffix to append after the integer
     * @return Pointer to the position after the appended integer and suffix
     */
    static char* appendUIntWithSuffix(char* buffer, uint32_t value, const __FlashStringHelper* suffix);

    /**
     * Template method to calculate the length of any type
     * @param value Value to calculate length for
     * @return Length in characters
     */
    template<typename T>
    static size_t calculateLength(const T& value) {
        // Generic implementation - convert to String and get length
        String str = String(value);
        return str.length();
    }

    /**
     * Template method to append any type directly to buffer
     * @param buffer Reference to current position in the char buffer
     * @param value Value to append (any type)
     * @return Pointer to the position after the appended value
     */
    template<typename T>
    static char* appendToBuffer(char*& buffer, const T& value) {
        // Generic implementation - convert to String and append
        String str = String(value);
        strcpy(buffer, str.c_str());
        buffer += str.length();
        return buffer;
    }
};

// Template specializations for efficient type handling
template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<const __FlashStringHelper*>(char*& buffer, const __FlashStringHelper* const& value) {
    const char* flashPtr = reinterpret_cast<const char*>(value);
    strcpy_P(buffer, flashPtr);
    buffer += strlen_P(flashPtr);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<String>(char*& buffer, const String& value) {
    strcpy(buffer, value.c_str());
    buffer += value.length();
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<int>(char*& buffer, const int& value) {
    buffer += sprintf(buffer, "%d", value);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<unsigned int>(char*& buffer, const unsigned int& value) {
    buffer += sprintf(buffer, "%u", value);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<long>(char*& buffer, const long& value) {
    buffer += sprintf(buffer, "%ld", value);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<unsigned long>(char*& buffer, const unsigned long& value) {
    buffer += sprintf(buffer, "%lu", value);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<float>(char*& buffer, const float& value) {
    buffer += sprintf(buffer, "%.2f", value);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<double>(char*& buffer, const double& value) {
    buffer += sprintf(buffer, "%.2f", value);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<bool>(char*& buffer, const bool& value) {
    const char* str = value ? "true" : "false";
    strcpy(buffer, str);
    buffer += strlen(str);
    return buffer;
}

template<>
inline char* DeviceFrameworkFlashStringUtils::appendToBuffer<IPAddress>(char*& buffer, const IPAddress& value) {
    String ipStr = value.toString();
    strcpy(buffer, ipStr.c_str());
    buffer += ipStr.length();
    return buffer;
}

// Template specializations for efficient length calculation
template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<const __FlashStringHelper*>(const __FlashStringHelper* const& value) {
    const char* flashPtr = reinterpret_cast<const char*>(value);
    return strlen_P(flashPtr);
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<String>(const String& value) {
    return value.length();
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<int>(const int& value) {
    // Estimate: int can be up to 11 characters (-2147483648)
    return 11;
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<unsigned int>(const unsigned int& value) {
    // Estimate: unsigned int can be up to 10 characters (4294967295)
    return 10;
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<long>(const long& value) {
    // Estimate: long can be up to 11 characters (-2147483648)
    return 11;
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<unsigned long>(const unsigned long& value) {
    // Estimate: unsigned long can be up to 10 characters (4294967295)
    return 10;
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<float>(const float& value) {
    // Estimate: float with 2 decimal places can be up to 15 characters
    return 15;
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<double>(const double& value) {
    // Estimate: double with 2 decimal places can be up to 15 characters
    return 15;
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<bool>(const bool& value) {
    // "true" = 4 chars, "false" = 5 chars
    return 5;
}

template<>
inline size_t DeviceFrameworkFlashStringUtils::calculateLength<IPAddress>(const IPAddress& value) {
    // IP addresses are always "xxx.xxx.xxx.xxx" = 15 chars max
    return 15;
}

#endif // DEVICEFRAMEWORK_FLASHSTRINGUTILS_H
