#include "HostnameUtils.h"
#include "../Configuration/DeviceFrameworkConfig.h"
#include <DeviceFrameworkPlatform.h>

String HostnameUtils::sanitizeHostname(const char* input, size_t maxLength) {
    if (!input) {
        return "";
    }

    String result = "";
    size_t inputLen = strlen(input);
    size_t maxLen = (maxLength > 0) ? maxLength : 63; // RFC 1123 limit

    for (size_t i = 0; i < inputLen && result.length() < maxLen; i++) {
        char c = input[i];

        if (isValidHostnameChar(c)) {
            // Convert to lowercase
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
            result += c;
        } else if (c == ' ' || c == '_' || c == '-' || c == '.') {
            // Replace spaces, underscores, hyphens, and dots with hyphens
            if (result.length() > 0 && result.charAt(result.length() - 1) != '-') {
                result += '-';
            }
        }
        // Skip other invalid characters
    }

    // Remove leading/trailing hyphens
    while (result.length() > 0 && result.charAt(0) == '-') {
        result = result.substring(1);
    }
    while (result.length() > 0 && result.charAt(result.length() - 1) == '-') {
        result = result.substring(0, result.length() - 1);
    }

    // Ensure it doesn't start with a number (some systems don't like this)
    if (result.length() > 0 && result.charAt(0) >= '0' && result.charAt(0) <= '9') {
        result = "esp-" + result;
    }

    return result;
}

String HostnameUtils::sanitizeHostname(const String& input) {
    return sanitizeHostname(input.c_str());
}

bool HostnameUtils::isValidHostname(const char* hostname) {
    if (!hostname || strlen(hostname) == 0) {
        return false;
    }

    size_t len = strlen(hostname);
    if (len > 63) { // RFC 1123 limit
        return false;
    }

    // Check each character
    for (size_t i = 0; i < len; i++) {
        if (!isValidHostnameChar(hostname[i])) {
            return false;
        }
    }

    return true;
}

bool HostnameUtils::isValidHostname(const String& hostname) {
    return isValidHostname(hostname.c_str());
}

String HostnameUtils::getSanitizedHostname(const char* input, const char* fallback) {
    // Sanitize the input first
    String sanitized = sanitizeHostname(input);

    // If result is empty, use fallback if provided, otherwise use default device name
    if (sanitized.length() == 0) {
        if (fallback) {
            return sanitizeHostname(fallback);
        }
        // No fallback provided - use sanitized default device name
        return sanitizeHostname(DF_DEFAULT_DEVICE_NAME);
    }

    return sanitized;
}

bool HostnameUtils::isValidHostnameChar(char c) {
    // Valid characters for hostnames: a-z, A-Z, 0-9, and hyphen
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c == '-');
}
