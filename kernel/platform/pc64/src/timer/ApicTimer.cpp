#include "ApicTimer.h"
#include "Hpet.h"
#include "../irq/LocalApic.h"
#include "../irq/ApicRegs.h"

#include <cpuid.h>
#include <arch/IrqRegistry.h>
#include <arch/PerCpuInfo.h>
#include <arch/spinlock.h>
#include <log.h>

using namespace platform;

bool ApicTimer::gLogInit        = true;
bool ApicTimer::gLogSet         = false;


/**
 * irq entry stub for the APIC timer
 */
void platform::ApicTimerIrq(const uintptr_t vector, void *ctx) {
    reinterpret_cast<ApicTimer *>(ctx)->fired();
}

/**
 * Initializes the local APIC core timer. We'll measure its speed against the PIT.
 */
ApicTimer::ApicTimer(LocalApic *_parent) : parent(_parent) {
    uint32_t eax, ebx, ecx, edx;
    // determine if this timer runs at a constant frequency
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    if(eax >= 0x06) {
        __get_cpuid(0x06, &eax, &ebx, &ecx, &edx);
        this->isConstantTime = (eax & (1 << 2));
    }

    // measure its frequency
    this->measureTimerFreq();
    if(gLogInit) log("APIC timer %3u: freq %lu Hz, constant time %c", this->parent->id, this->freq,
            this->isConstantTime ? 'Y' : 'N');

    // install the irq handler
    auto irq = arch::PerCpuInfo::get()->irqRegistry;
    irq->install(kVector, ApicTimerIrq, this);
}

/**
 * Turns off the APIC timer again and removes its interrupt.
 */
ApicTimer::~ApicTimer() {
    // mask timer LVT
    auto lvt = this->parent->read(kApicRegLvtTimer);
    lvt |=  (1 << 16);
    this->parent->write(kApicRegLvtTimer, lvt);

    // remove interrupt handler
    auto irq = arch::PerCpuInfo::get()->irqRegistry;
    irq->remove(kVector);
}



/**
 * The timer has fired; increment the per-core timebase and do whatever else needs doing.
 */
void ApicTimer::fired() {
    log("apic thymer fired");

    // acknowledge interrupt
    this->parent->eoi();
}

/**
 * Measure the timer frequency against the system HPET. This is done a configurable number of times
 * then averaged.
 */
void ApicTimer::measureTimerFreq() {
    // prepare the timer to use a 16x divider and set up our averages
    this->parent->write(kApicRegTimerDivide, 0b0011);

    uint64_t ps[kTimeAverages] {0};

    // measure the timer N number of tim es (for 10ms each time)
    for(size_t i = 0; i < kTimeAverages; i++) {
        // start APIC timer and wait
        this->parent->write(kApicRegTimerInitial, 0xFFFFFFFF); // initial counter is -1
        const auto actualPicos = Hpet::the()->busyWait(1000ULL * 1000ULL * 10) * 1000ULL;

        // stop APIC timer and read out its tick value
        const auto currentTimer = this->parent->read(kApicRegTimerCurrent);
        this->parent->write(kApicRegTimerInitial, 0);

        const auto ticksPerTime = 0xFFFFFFFF - currentTimer;

        // store the iteration's picosecond value
        ps[i] = actualPicos / ticksPerTime;
    }

    // calculate average picosecond value (and frequency)
    uint64_t psAvg = 0;
    for(size_t i = 0; i < kTimeAverages; i++) {
        psAvg += ps[i];
    }

    this->psPerTick = psAvg / kTimeAverages;

    const uint64_t psPerClock = this->psPerTick / 16;
    this->freq = (1000000000000 / psPerClock);

    if(gLogInit) {
        log("APIC timer %3u: %lu ps per tick (avg) freq %lu Hz", this->parent->id,
                psPerTick, this->freq);
    }
}

/**
 * Configures the timer in one-shot mode with the given interval.
 *
 * @param nsec Interval for the timer, in nanoseconds
 * @param repeat Whether the timer is in one-shot (`false`) or repeating (`true`) mode
 *
 * @return The actually achieved time set
 */
uint64_t ApicTimer::setInterval(const uint64_t nsec, const bool repeat) {
    // convert to period
    const uint64_t divisor = 16; // XXX: should this be supported to change?
    const uint64_t ticks = ((nsec * 1000ULL) / (this->psPerTick * divisor));

    if(gLogSet) log("ps per tick %lu ticks %lu", nsec, ticks);

    // mask existing timer interrupt
    auto lvt = this->parent->read(kApicRegLvtTimer);
    lvt |=  (1 << 16);
    lvt &= ~(0b11 << 17); // clear timer type
    lvt |=  (repeat ? 0b01 : 0b00) << 17;

    this->parent->write(kApicRegLvtTimer, lvt);

    // write the timer configuration
    this->parent->write(kApicRegTimerDivide, 0b0011); // divide by 16
    this->parent->write(kApicRegTimerInitial, ticks);
    this->ticksForInterval = ticks;

    // unmask timer interrupt
    uint32_t lvtValue = kVector;
    lvtValue |= (repeat ? 0b01 : 0b00) << 17;
    this->parent->write(kApicRegLvtTimer, lvtValue);

    // return what we've actually achieved
    this->intervalPs = (ticks * psPerTick * divisor);
    return this->intervalPs / 1000ULL;
}
