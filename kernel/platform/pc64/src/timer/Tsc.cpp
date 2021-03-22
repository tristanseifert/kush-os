#include "Tsc.h"
#include "Hpet.h"

#include <arch/PerCpuInfo.h>
#include <log.h>

using namespace platform;

bool Tsc::gLogInit      = true;

/**
 * Initializes the current core's TSC.
 *
 * When this is called, the scheduler isn't running, so we need not worry about being moved
 * between cores and that fucking up our measurements.
 */
void Tsc::InitCoreLocal() {
    auto tsc = new Tsc;
    arch::PerCpuInfo::get()->p.tsc = tsc;
}

/**
 * Return the current core's TSC timer.
 */
Tsc *Tsc::the() {
    return arch::PerCpuInfo::get()->p.tsc;
}

/**
 * Measures the TSC frequency against the HPET. This is then averaged a few times to provide a
 * more accurate result.
 *
 * Additionally, if the core supports the `RDTSCP` feature, we configure the per-core MSR with the
 * core's ID. This can be used to look up the TSC pointer from that core's per-core info block, in
 * order to convert its times and so on.
 */
Tsc::Tsc() {
    // measure 
    uint64_t ps[kTimeAverages] {0};

    // measure the timer N number of tim es (for 10ms each time)
    for(size_t i = 0; i < kTimeAverages; i++) {
        const auto start = GetCount();
        const auto actualPicos = Hpet::the()->busyWait(1000ULL * kTimeMeasure) * 1000ULL;
        const auto ticksPerTime = GetCount() - start;

        // store the iteration's picosecond value
        ps[i] = actualPicos / ticksPerTime;
    }

    // calculate average picosecond value (and frequency)
    uint64_t psAvg = 0;
    for(size_t i = 0; i < kTimeAverages; i++) {
        psAvg += ps[i];
    }

    this->psPerTick = psAvg / kTimeAverages;

    const uint64_t psPerClock = this->psPerTick;
    this->freq = (1000000000000ULL / psPerClock);

    if(gLogInit) {
        const auto coreId = arch::PerCpuInfo::get()->procId;
        log("TSC core %3u: %lu ps (%lu Hz)", coreId, this->psPerTick, this->freq);
    }

    // TODO: set MSRs
}

/**
 * Performs a busy wait.
 *
 * @return Actual number of nanoseconds we waited for.
 */
uint64_t Tsc::busyWait(const uint64_t nsec) {
    uint64_t actualNs, ticksToWait, start = 0, ticks = 0;

    // get the ticks
    ticksToWait = this->nsToTicks(nsec, actualNs);

    // wait
    start = GetCount();

    do {
        ticks = GetCount() - start;
    } while(ticks < ticksToWait);

    // done
    return actualNs;
}


/**
 * Converts a number of TSC ticks into nanoseconds.
 */
uint64_t Tsc::ticksToNs(const uint64_t ticks) const {
    return (reinterpret_cast<uint64_t>(ticks) * this->psPerTick) / 1000ULL;
}

/**
 * Converts a number of nanoseconds into TSC ticks.
 *
 * This should always be able to accomodate all nanosecond requests exactly; most TSCs run at
 * frequencies greater than 1GHz.
 */
uint64_t Tsc::nsToTicks(const uint64_t desiredNs, uint64_t &actualNs) const {
    const uint64_t desiredPs = (desiredNs * 1000ULL);
    const auto desiredTicks = desiredPs / static_cast<uint64_t>(this->psPerTick);

    actualNs = (desiredTicks * static_cast<uint64_t>(this->psPerTick)) / 1000ULL;

    return desiredTicks;
}

