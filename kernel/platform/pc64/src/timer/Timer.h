#ifndef PLATFORM_PC64_TIMER_TIMER_H
#define PLATFORM_PC64_TIMER_TIMER_H

#include <stdint.h>

namespace platform {
/**
 * Base interface for platform exposed timers. These are implemented as up/down counters with a
 * fixed frequency.
 */
class Timer {
    public:
        /// Converts timer ticks into nanoseconds
        virtual uint64_t ticksToNs(const uint64_t ticks) const = 0;
        /// Converts nanoseconds to timer ticks
        virtual uint64_t nsToTicks(const uint64_t nsec, uint64_t &actualNsec) const = 0;

        /// Performs a busy wait for the given time using the HPET as a reference.
        virtual uint64_t busyWait(const uint64_t nsec) = 0;
};
}

#endif
