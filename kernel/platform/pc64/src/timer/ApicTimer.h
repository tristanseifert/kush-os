#ifndef PLATFORM_PC64_TIMER_APICTIMER_H
#define PLATFORM_PC64_TIMER_APICTIMER_H

#include <stddef.h>
#include <stdint.h>

namespace platform {
class LocalApic;

/**
 * Each core-local APIC contains a high-resolution (typically, on the order of tens of nanoseconds
 * of precision) timer that's used for things like the scheduler and other core-local timing.
 *
 * An interface is exposed to use the timer in one-shot (deadline) mode.
 */
class ApicTimer {
    friend void ApicTimerIrq(const uintptr_t, void *);

    public:
        /// IRQ vector for the timer
        constexpr static const uint8_t kVector = 0x2F;

    public:
        ApicTimer(LocalApic *parent);
        ~ApicTimer();

        /// Set the interval (in ns) until the timer fires
        uint64_t setInterval(const uint64_t nsec, const bool repeat = false);
        /// Stops the timer
        void stop();

    private:
        /// interrupt callback indicating the timer was triggered
        void fired();
        /// Measures the frequency of the timer by comparing against the PIT
        void measureTimerFreq();

    private:
        /// Number of times we'll measure the APIC timer and average it
        constexpr static const size_t kTimeAverages = 5;

        /// are the initializations of the timer logged?
        static bool gLogInit;
        /// are timer interval changes logged?
        static bool gLogSet;

    private:
        /// APIC that owns us
        LocalApic *parent;

        /// ticks for the interval
        uint32_t ticksForInterval = 0;
        /// Current timer interval (in nanoseconds)
        uint64_t intervalPs = 0;

        /// Timer input frequency (in Hz)
        uint64_t freq = 0;
        /// picoseconds per tick
        uint64_t psPerTick = 0;

        /// determine whether the timer always runs at a constant rate, regardless of P-states
        bool isConstantTime = false;
};
}

#endif
