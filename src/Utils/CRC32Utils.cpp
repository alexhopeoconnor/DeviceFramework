#include "CRC32Utils.h"

uint32_t CRC32Utils::calculate(const uint8_t* data, size_t length) {
    uint32_t crc = initialValue();
    while (length--) {
        crc = update(crc, *data++);
    }
    return crc;
}

uint32_t CRC32Utils::initialValue() {
    return 0xffffffff;
}

uint32_t CRC32Utils::update(uint32_t crc, uint8_t value) {
    for (uint32_t bitMask = 0x80; bitMask > 0; bitMask >>= 1) {
        bool bit = crc & 0x80000000;
        if (value & bitMask) bit = !bit;
        crc <<= 1;
        if (bit) crc ^= 0x04C11DB7;
    }
    return crc;
}
