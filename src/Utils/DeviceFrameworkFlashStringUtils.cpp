#include "DeviceFrameworkFlashStringUtils.h"

char* DeviceFrameworkFlashStringUtils::appendFlashString(char* buffer, const __FlashStringHelper* flashStr) {
    const char* flashPtr = reinterpret_cast<const char*>(flashStr);
    size_t len = strlen_P(flashPtr);
    strcpy_P(buffer, flashPtr);
    return buffer + len;
}

char* DeviceFrameworkFlashStringUtils::appendString(char* buffer, const String& str) {
    strcpy(buffer, str.c_str());
    return buffer + str.length();
}

char* DeviceFrameworkFlashStringUtils::appendInt(char* buffer, int32_t value) {
    return buffer + sprintf(buffer, "%d", value);
}

char* DeviceFrameworkFlashStringUtils::appendUInt(char* buffer, uint32_t value) {
    return buffer + sprintf(buffer, "%u", value);
}

char* DeviceFrameworkFlashStringUtils::appendIntWithSuffix(char* buffer, int32_t value, const __FlashStringHelper* suffix) {
    const char* suffixPtr = reinterpret_cast<const char*>(suffix);
    int len = sprintf(buffer, "%d", value);
    strcpy_P(buffer + len, suffixPtr);
    return buffer + len + strlen_P(suffixPtr);
}

char* DeviceFrameworkFlashStringUtils::appendUIntWithSuffix(char* buffer, uint32_t value, const __FlashStringHelper* suffix) {
    const char* suffixPtr = reinterpret_cast<const char*>(suffix);
    int len = sprintf(buffer, "%u", value);
    strcpy_P(buffer + len, suffixPtr);
    return buffer + len + strlen_P(suffixPtr);
}
