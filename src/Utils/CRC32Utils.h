#ifndef CRC32_UTILS_H
#define CRC32_UTILS_H

#include <Arduino.h>

class CRC32Utils {
public:
    static uint32_t calculate(const uint8_t* data, size_t length);
};

#endif // CRC32_UTILS_H
