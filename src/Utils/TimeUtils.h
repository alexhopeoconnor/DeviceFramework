#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

class TimeUtils {
public:
    // Calculate safe time difference handling millis() overflow
    // Returns the difference in milliseconds between current and previous timestamps
    // Handles the case where millis() overflows (every ~49.7 days)
    static unsigned long safeTimeDifference(unsigned long current, unsigned long previous);

    // Check if enough time has elapsed since a previous timestamp
    // Returns true if (current - previous) >= interval, handling overflow
    static bool hasTimeElapsed(unsigned long current, unsigned long previous, unsigned long interval);

    // Get remaining time until a target timestamp
    // Returns milliseconds remaining, or 0 if time has already passed
    static unsigned long getRemainingTime(unsigned long current, unsigned long target);

    // Check if a timestamp is in the future (handles overflow)
    // Returns true if target > current, accounting for millis() overflow
    static bool isInFuture(unsigned long current, unsigned long target);
};

#endif // TIME_UTILS_H
