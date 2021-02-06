#include "Apic.h"

#include "../memmap.h"

#include <arch/x86_msr.h>

#include <vm/Mapper.h>
#include <vm/Map.h>
#include <log.h>

using namespace platform::irq;

/**
 * Reads out the APIC base MSR.
 */
static uint32_t GetApicBase() {
    uint32_t lo, hi;
    x86_msr_read(IA32_APIC_BASE_MSR, &lo, &hi);

    return lo;
}
/**
 * Writes the APIC base MSR.
 */
static void SetApicBase(const uint32_t base) {
    x86_msr_write(IA32_APIC_BASE_MSR, base, 0);
}

/**
 * Initializes a new APIC.
 *
 * XXX: Since APICs are core-local, this should really be carefully ensured to execute on each
 * of those cores. Since we don't do SMP yet, this isn't a problem.
 */
Apic::Apic(void *_virtBase, const uint8_t _cpuId, const uint8_t _id, const bool _enabled,
        const bool _onlineable) : id(_id), processor(_cpuId),
        base(reinterpret_cast<uint32_t *>(_virtBase)) {
    REQUIRE(_virtBase, "Invalid APIC virtual base address: %p", _virtBase);

    log("New APIC %u (processor %u) enabled %c", this->id, this->processor, _enabled ? 'Y':'N');

    // check if we're a BSP
    const auto base = GetApicBase();
    this->isBsp = (base & IA32_APIC_BASE_MSR_BSP);
}

/**
 * Enables this APIC.
 */
void Apic::enable() {
    // enable by writing MSR
    const auto base = GetApicBase();
    SetApicBase(base);

    // set spurious interrupt register
    auto reg = this->base[0xF0 / 4] & ~0xFF;
    reg |= 0xFF; // spurious vector is 0xFF
    reg |= (1 << 8); // set enable bit
    this->base[0xF0 / 4] = reg;
}
/**
 * Signals an end-of-interrupt for the APIC.
 */
void Apic::endOfInterrupt() {
    this->base[0xB0 / 4] = 0;
}

/**
 * Maps one of the local interrupts as an NMI.
 */
void Apic::mapNmi(const uint8_t lint, const IrqFlags f) {
    REQUIRE((lint == 0) || (lint == 1), "Invalid APIC local interrupt %u", lint);

    // build the register value
    uint32_t value = kVectorNMI;
    value |= (0b100 << 8); // deliver as NMI
    value |= flags(f & IrqFlags::PolarityLow) ? (1 << 13) : 0;

    // write it to the appropriate register
    if(lint == 0) {
        this->base[0x350 / 4] = value;
    } 
    else if(lint == 1) {
        this->base[0x360 / 4] = value;
    }
}
