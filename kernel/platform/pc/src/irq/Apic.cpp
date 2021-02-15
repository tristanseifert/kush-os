#include "Apic.h"
#include "ApicRegs.h"

#include "memmap.h"
#include "timer/LocalApicTimer.h"

#include <arch/x86_msr.h>
#include <arch/spinlock.h>
#include <vm/Mapper.h>
#include <vm/Map.h>
#include <log.h>
#include <cpuid.h>

using namespace platform::irq;

static Irql gIrql = Irql::Passive;

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
 * Cleanly shuts down the APIC.
 */
Apic::~Apic() {
    // shut down the timer
    delete this->timer;

    // clear the enable bit for the APIC
    auto reg = this->read(kApicRegSpurious);
    reg &= ~(1 << 8); // set enable bit
    this->write(kApicRegSpurious, reg);
}

/**
 * Enables this APIC.
 */
void Apic::enable() {
    // enable by writing MSR
    const auto base = GetApicBase();
    SetApicBase(base);

    // set spurious interrupt register
    auto reg = this->read(kApicRegSpurious) & ~0xFF;
    reg |= 0xFF; // spurious vector is 0xFF
    reg |= (1 << 8); // set enable bit
    this->write(kApicRegSpurious, reg);

    // set up the local timer
    this->timer = new timer::LocalApicTimer(this);
}
/**
 * Signals an end-of-interrupt for the APIC.
 */
void Apic::endOfInterrupt() {
    this->write(kApicRegEndOfInt, 0);
}

/**
 * Maps one of the local interrupts as an NMI.
 */
void Apic::mapNmi(const uint8_t lint, const IrqFlags f) {
    REQUIRE((lint == 0) || (lint == 1), "Invalid APIC local interrupt %u", lint);

    // build the register value
    uint32_t value = kVectorNMI;
    value |= (0b100 << 8); // deliver as NMI
    value |= TestFlags(f & IrqFlags::PolarityLow) ? (1 << 13) : 0;

    // write it to the appropriate register
    if(lint == 0) {
        this->write(kApicRegLvtLint0, value);
    } 
    else if(lint == 1) {
        this->write(kApicRegLvtLint1, value);
    }
}

/**
 * Sends a dispatch IPI to ourselves.
 *
 * This will send a fixed priority interrupt to only ourselves.
 */
void Apic::sendDispatchIpi() {
    uint32_t command = kVectorDispatch;
    command |= (1 << 14); // level is always 1
    command |= (0b01 << 18); // shorthand = self

    this->write(kApicRegInterruptCmdLow, command);
}

/**
 * Update the APIC task priority register.
 */
void Apic::updateTpr(const Irql irql) {
    uint8_t priority;

    switch(irql) {
        case Irql::CriticalSection:
            priority = 0xFF;
            break;
        case Irql::IPI:
            priority = 0xC0;
            break;
        case Irql::Clock:
            priority = 0xB0;
            break;
        case Irql::DeviceIrq:
            priority = 0x30;
            break;
        case Irql::Dpc:
        case Irql::Scheduler:
            priority = 0x20;
            break;
        case Irql::Passive:
            priority = 0;
            break;

        default:
            panic("unhandled irql %d", (int) irql);
    }

    this->write(kApicRegTaskPriority, (uint32_t) priority);
}

/**
 * Raises the interrupt priority level of the current processor.
 */
platform::Irql platform_raise_irql(const Irql irql, const bool enableIrq) {
    Irql prev, newVal = irql;

    asm volatile("cli");
    REQUIRE(irql >= gIrql, "cannot %s irql: current %d, requested %d", "raise", (int) gIrql,
            (int) irql);
    __atomic_exchange(&gIrql, &newVal, &prev, __ATOMIC_ACQUIRE);

    Manager::currentProcessorApic()->updateTpr(irql);

    if(enableIrq) {
        asm volatile("sti");
    }

    return prev;
}

/**
 * Lowers the interrupt priority level of the current processor.
 */
void platform_lower_irql(const platform::Irql irql, const bool enableIrq) {
    asm volatile("cli");
    REQUIRE(irql <= gIrql, "cannot %s irql: current %d, requested %d", "lower", (int) gIrql,
            (int) irql);
    gIrql = irql;

    Manager::currentProcessorApic()->updateTpr(irql);

    if(enableIrq) {
        asm volatile("sti");
    }
}

/**
 * Returns the current irql.
 */
const platform::Irql platform_get_irql() {
    return gIrql;
}

/**
 * Requests a dispatch IPI to be sent to the current processor.
 */
void platform_request_dispatch() {
    auto apic = Manager::currentProcessorApic();
    apic->sendDispatchIpi();
}
