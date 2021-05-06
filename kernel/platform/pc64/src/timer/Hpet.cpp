#include "Hpet.h"
#include "../memmap.h"
#include "../acpi/Parser.h"

#include <arch.h>
#include <sched/Task.h>
#include <vm/Map.h>
#include <vm/MapEntry.h>

#include <log.h>

using namespace platform;

Hpet *Hpet::gShared = nullptr;

bool Hpet::gLogInit     = false;

/**
 * Scan the ACPI table to find a HPET descriptor table. With the first table, create the system
 * global HPET.
 *
 * Note that it's mandatory the machine has one of these: basically everything that supports x86_64
 * should have one, especially considering we're limiting hardware support to Nehalem and later
 * regardless.
 */
void Hpet::Init() {
    // ensure system has a HPET
    auto info = AcpiParser::the()->hpetInfo;
    REQUIRE(info, "no HPET found in ACPI tables");

    // initialize it
    gShared = new Hpet(info->address.physAddr, info);
}

/**
 * Initializes a HPET with the given physical base address. The ACPI table is passed as well, but
 * we currently ignore this in lieu of simply reading from the hardware registers.
 *
 * XXX: It's possible the firmware may give us non-aligned physical addressed.
 */
Hpet::Hpet(const uintptr_t phys, const void *table) {
    int err;

    REQUIRE(!(phys % arch_page_size()), "HPET phys base $%p not page aligned", phys);

    // map the controller
    this->vm = vm::MapEntry::makePhys(phys, arch_page_size(),
            vm::MappingFlags::Read | vm::MappingFlags::Write | vm::MappingFlags::MMIO, true);
    REQUIRE(this->vm, "failed to create %s phys map", "HPET");

    auto map = vm::Map::kern();
    err = map->add(this->vm, sched::Task::kern(), 0, vm::MappingFlags::None, kPlatformRegionMmio,
            (kPlatformRegionMmio + kPlatformRegionMmioLen - 1));
    REQUIRE(!err, "failed to map %s: %d", "HPET", err);

    auto vmBase = map->getRegionBase(this->vm);
    REQUIRE(vmBase, "failed to get %s base address", "HPET");

    this->base = reinterpret_cast<void *>(vmBase);
    if(gLogInit) log("HPET base: $%p", this->base);

    // read out vendor and clock period
    const auto gcid = this->read64(kRegGcId);
    const auto rev = (gcid & 0xFF);

    this->period = (gcid >> 32);
    this->numTimers = (gcid & 0x1F00) >> 8;
    if(gLogInit) log("HPET period: %u fs (vendor $%04x rev %u) with %u timers", this->period,
            (gcid & 0xFFFF0000) >> 16, rev, this->numTimers);

    REQUIRE(gcid & (1 << 13), "HPET is not 64-bit capable (GCID $%016llx)", gcid);

    // ensure the timer is enabled and legacy replacement routing is disabled
    auto gconf = this->read64(kRegGConf);
    gconf &= ~(1 << 0);
    this->write64(kRegGConf, gconf);

    gconf &= ~(1 << 1);
    gconf |= (1 << 0);
    this->write64(kRegGConf, gconf);

    // self test: ensure it's actually started
    const auto time1 = this->getCount();
    for(int i = 0; i < 100000; i++) {
        // nothing
    }

    const auto time2 = this->getCount();
    REQUIRE(time2 > time1, "HPET is broken: %u %u", time1, time2);

    this->countOffset = this->getCount();
}

/**
 * Unmap the HPET from the system's VM space.
 */
Hpet::~Hpet() {
    int err;

    // remove VM mapping
    auto map = vm::Map::kern();
    err = map->remove(this->vm, sched::Task::kern());
    REQUIRE(!err, "failed to unmap %s phys map", "HPET");
}

/**
 * Performs a busy wait, using the HPET as the time reference. This can be called by multiple
 * cores simultaneously, since we only read from the HPET's count register.
 *
 * @note Interrupts are _not_ automatically disabled by this routine.
 *
 * @return Actual number of nanoseconds slept (sans overhead)
 */
uint64_t Hpet::busyWait(const uint64_t nsec) {
    uint64_t actualNs, ticksToWait, start = 0, ticks = 0;

    // get the ticks
    ticksToWait = this->nsToTicks(nsec, actualNs);

    // wait
    start = this->getCount();

    do {
        ticks = this->getCount() - start;
    } while(ticks < ticksToWait);

    // done
    return actualNs;
}



/**
 * Converts an interval in timer ticks to nanoseconds.
 *
 * This will break if the duration is much longer than about five hours: this is the number of
 * femtoseconds that fit in a 64-bit quantity. In the future, it may be worth doing 128-bit math,
 * though that depends on the compiler built-in libraries.
 */
uint64_t Hpet::ticksToNs(const uint64_t ticks) const {
    uint64_t fs = ticks;
    fs *= static_cast<uint64_t>(this->period);

    return (fs / 1000ULL / 1000ULL);
}

/**
 * Converts a given number of nanoseconds to a number of ticks of the HPET timer. This will always
 * round down.
 *
 * As with `ticksToNs()`, this is only good for up to about five hour waits.
 */
uint64_t Hpet::nsToTicks(const uint64_t desiredNs, uint64_t &actualNs) const {
    const uint64_t desiredFs = (desiredNs * 1000ULL * 1000ULL);
    const auto desiredTicks = desiredFs / static_cast<uint64_t>(this->period);

    actualNs = (desiredTicks * static_cast<uint64_t>(this->period)) / 1000ULL / 1000ULL;

    return desiredTicks;
}


/**
 * Uses the HPET to get the nanoseconds since boot.
 *
 * XXX: This is pretty slow; we should be using the TSC if at all possible instead.
 */
uint64_t platform_timer_now() {
    auto hpet = Hpet::the();
    if(hpet) [[likely]] {
        return hpet->ticksToNs(hpet->ticksSinceInit());
    }

    return 0;
}

