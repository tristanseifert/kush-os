#ifndef PLATFORM_PC64_TIMER_TSC_H
#define PLATFORM_PC64_TIMER_TSC_H

#include <stddef.h>
#include <stdint.h>

#include <arch/PerCpuInfo.h>

#include "Timer.h"

namespace platform {
/**
 * Provides a thin wrapper around the processor-local time-stamp counter.
 *
 * There's no _clean_ way to get the actual rate of the counter, so we measure it against the HPET
 * during initialization.
 */
class Tsc: public Timer {
    public:
        /// Initializes the TSC for the current processor.
        static void InitCoreLocal();

        /// Return the core local TSC timer
        static inline Tsc *the() {
            return arch::PerCpuInfo::get()->p.tsc;
        }

        /// Reads the TSC of the current core.
        static inline uint64_t GetCount() {
            return __rdtsc();
        }


        /// Converts a TSC count into nanoseconds
        uint64_t ticksToNs(const uint64_t ticks) const;
        /// Converts nanoseconds to TSC counts
        uint64_t nsToTicks(const uint64_t nsec, uint64_t &actualNsec) const;

        /// Performs a busy wait for the given time using the TSC as a reference.
        uint64_t busyWait(const uint64_t nsec);

    private:
        /// Interval (in microseconds) to measure the TSC for
        constexpr static const unsigned long kTimeMeasure = 10000ULL;
        /// Number of averages to take of the TSC frequency
        constexpr static const size_t kTimeAverages = 10;

        /// Whether the initialization of the TSC is logged
        static bool gLogInit;

    private:
        Tsc();

    private:
        /// Timer input frequency (in Hz)
        uint64_t freq = 0;
        /// picoseconds per tick
        uint64_t psPerTick = 0;
};
}

#endif
