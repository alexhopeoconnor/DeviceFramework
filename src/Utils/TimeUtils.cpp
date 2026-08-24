#include "TimeUtils.h"
#include <climits>

unsigned long TimeUtils::safeTimeDifference(unsigned long current, unsigned long previous) {
    // Handle millis() overflow (occurs every ~49.7 days)
    if (current >= previous) {
        return current - previous;
    } else {
        // Overflow occurred - calculate the difference correctly
        // Time from previous to overflow + time from overflow to current
        return (ULONG_MAX - previous) + current + 1;
    }
}

bool TimeUtils::hasTimeElapsed(unsigned long current, unsigned long previous, unsigned long interval) {
    return safeTimeDifference(current, previous) >= interval;
}

unsigned long TimeUtils::getRemainingTime(unsigned long current, unsigned long target) {
    if (current >= target) {
        // Target is in the past or now
        return 0;
    } else {
        // Check if target is in the future (accounting for overflow)
        if (isInFuture(current, target)) {
            return target - current;
        } else {
            // Target has already passed due to overflow
            return 0;
        }
    }
}

bool TimeUtils::isInFuture(unsigned long current, unsigned long target) {
    // If target > current, it's normally in the future
    // But we need to check if this is due to overflow
    if (target > current) {
        // Check if the difference is reasonable (not due to overflow)
        // If target is much larger than current, it might be from before overflow
        unsigned long diff = target - current;
        // If difference is more than half the max value, likely an overflow case
        return diff < (ULONG_MAX / 2);
    } else {
        // target <= current, so it's not in the future
        return false;
    }
}
