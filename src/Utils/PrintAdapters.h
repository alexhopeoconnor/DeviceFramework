#ifndef DEVICEFRAMEWORK_PRINT_ADAPTERS_H
#define DEVICEFRAMEWORK_PRINT_ADAPTERS_H

#include <Arduino.h>

/**
 * PrintAdapters - Utility classes for adapting Print interface to different targets
 *
 * These adapters allow using the Print interface with different output targets,
 * enabling memory-efficient streaming without String concatenations.
 */

/**
 * PrintStringAdapter - Adapts Print interface to append to a String
 *
 * This adapter writes all Print output directly to a String, avoiding
 * intermediate String allocations from concatenation operations.
 *
 * Usage:
 *   String output;
 *   output.reserve(512);
 *   PrintStringAdapter adapter(output);
 *   adapter.print(F("Hello"));
 *   adapter.print(42);
 */
class PrintStringAdapter : public Print {
private:
    String& str;

public:
    explicit PrintStringAdapter(String& s) : str(s) {}

    size_t write(uint8_t c) override {
        str += (char)c;
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        for (size_t i = 0; i < size; i++) {
            str += (char)buffer[i];
        }
        return size;
    }
};

/**
 * CountingPrintAdapter - Adapts Print interface to count bytes without storing them
 *
 * This adapter counts all bytes written without allocating any memory,
 * useful for calculating output size before building the actual output.
 *
 * Usage:
 *   CountingPrintAdapter counter;
 *   counter.print(F("Hello"));
 *   counter.print(42);
 *   size_t totalSize = counter.getCount();
 */
class CountingPrintAdapter : public Print {
private:
    size_t count;

public:
    CountingPrintAdapter() : count(0) {}

    size_t write(uint8_t c) override {
        count++;
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        count += size;
        return size;
    }

    size_t getCount() const {
        return count;
    }

    void reset() {
        count = 0;
    }
};

#endif // DEVICEFRAMEWORK_PRINT_ADAPTERS_H
