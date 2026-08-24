#ifndef HOSTNAME_UTILS_H
#define HOSTNAME_UTILS_H

#include <Arduino.h>
#include "../DeviceFrameworkDebug.h"

class HostnameUtils {
public:
    // Sanitize hostname to be valid for mDNS, WiFi, and OTA
    // Returns empty string if input is null/empty or sanitization results in empty
    static String sanitizeHostname(const char* input, size_t maxLength = 63);
    static String sanitizeHostname(const String& input);

    // Validate if a hostname is valid
    static bool isValidHostname(const char* hostname);
    static bool isValidHostname(const String& hostname);

    // Get sanitized hostname with fallback
    static String getSanitizedHostname(const char* input, const char* fallback = "esp8266");

private:
    // Character validation
    static bool isValidHostnameChar(char c);
};

#endif // HOSTNAME_UTILS_H
