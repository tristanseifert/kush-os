#ifndef PLATFORM_PC_TIMER_LOCALAPICTIMER_H
#define PLATFORM_PC_TIMER_LOCALAPICTIMER_H

#include <stdint.h>

namespace platform {
namespace irq {
    class Apic;
}

namespace timer {
/**
 * All APICs have a built-in timer; this class exposes an interface to it.
 *
 * Note that these timers are local to the core the APIC belongs to.
 */
class LocalApicTimer {
    friend class irq::Apic;

    public:
        /// Vector number for the APIC timer interrupt
        constexpr static const uint8_t kTimerVector = 0xB0;

    public:
        /// Gets the frequency of the timer.
        const float getFrequency() {
            return this->freq;
        }

        /// Reads out the number of ticks remaining in the count.
        const uint64_t ticksRemaining();
        /// Returns the number of nanoseconds that have elapsed in the tick.
        const uint64_t nsInTick();

        /// Sets the interval at which this timer fires an interrupt.
        const float setInterval(const float usecs);

    private:
        LocalApicTimer(irq::Apic *parent);

        void measureTimerFreq();

    private:
        /// APIC that this timer is a part of
        irq::Apic *apic = nullptr;

        /// ticks for the interval
        uint32_t ticksForInterval = 0;
        /// currently configured interval (in nanoseconds)
        uint64_t intervalNs = 0;
        /// input frequency of the timer (in kHz)
        float freq = 0;
};
}}

#endif
