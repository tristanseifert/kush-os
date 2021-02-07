#include "LocalApicTimer.h"
#include "pit.h"

#include "irq/Apic.h"
#include "irq/ApicRegs.h"

#include <arch/spinlock.h>
#include <log.h>

using namespace platform::timer;

/**
 * Since we have only one PIT, but multiple APICs (if there's many cores) we need to be sure that
 * only one core is using it to calculate the APIC tick frequency at a time.
 */
DECLARE_SPINLOCK_S(gPitLock);

/**
 * Initializes the local APIC timer.
 */
LocalApicTimer::LocalApicTimer(irq::Apic *parent) : apic(parent) {
    // calculate timer frequency
    this->measureTimerFreq();
}

/**
 * Attempts to determine the clock frequency of the core-local APIC timer. We do this by using the
 * timer in 16x divide mode, as an upcounter, comparing it against a measured 10ms on the PIT
 * that we know the system has.
 *
 * This should only be used if we can't determine the frequency by other means, e.g. ACPI tables or
 * relevant CPUID leaves.
 */
void LocalApicTimer::measureTimerFreq() {
    float actualMicros = 0;

    // prepare the timer to use a 16x divider
    this->apic->write(kApicRegTimerDivide, 0b0011);

    // configure PIT
    SPIN_LOCK_GUARD(gPitLock);
    actualMicros = timer::LegacyPIT::configBusyWait(10000);

    // start APIC timer, wait on PIT to elapse 10ms
    this->apic->write(kApicRegTimerInitial, 0xFFFFFFFF); // initial counter is -1
    timer::LegacyPIT::busyWait();

    // stop APIC timer and read out its tick value
    const auto currentTimer = this->apic->read(kApicRegTimerCurrent);
    this->apic->write(kApicRegTimerInitial, 0);

    const auto ticksPerTime = 0xFFFFFFFF - currentTimer;

    // calculate the time a tick takes
    const double nsPerTick = (actualMicros * 1000.) / ((double) ticksPerTime);
    const double nsPerClock = nsPerTick / 16.;

    this->freq = (1000. / nsPerClock) * 1000.;
    //log("APIC timer value: %08lx %08lx ticks, %g ns per tick", currentTimer, ticksPerTime, nsPerTick);
}

/**
 * Configures the interval at which this timer generates interrupts.
 *
 * @return The actually achieved interval.
 */
const float LocalApicTimer::setInterval(const float usecs) {
    // TODO: dynamically change divisor?
    const auto divisor = 16.;
    const double nsPerTick = (1. / (this->freq * 1000.)) * 1000. * 1000. * 1000.;
    const auto ticks = ((usecs * 1000.) / (nsPerTick * divisor));

    // log("ns per tick %g, %g ticks", nsPerTick, ticks);

    // mask existing timer interrupt
    auto lvt = this->apic->read(kApicRegLvtTimer);
    lvt |= (1 << 16);
    lvt &= ~(0b11 << 17);
    lvt |= (0b01 << 17);

    this->apic->write(kApicRegLvtTimer, lvt);

    // write the timer configuration
    this->apic->write(kApicRegTimerDivide, 0b0011); // divide by 16
    this->apic->write(kApicRegTimerInitial, ticks);

    // unmask timer interrupt
    uint32_t lvtValue = kTimerVector;
    lvtValue |= (0b01 << 17);
    this->apic->write(kApicRegLvtTimer, lvtValue);

    // return what we've actually achieved
    return (ticks * nsPerTick * divisor) / 1000.;
}

