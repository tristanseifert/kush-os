#include "IoApic.h"

#include "memmap.h"

#include <vm/Mapper.h>
#include <vm/Map.h>
#include <log.h>

// whether to log remapped IRQs
#define LOG_IRQ_REMAP                   0

// IOAPIC ID register
#define IOAPICID                        0x00
// Version register
#define IOAPICVER                       0x01
// Arbitration id
#define IOAPICARB                       0x02
// low 32 bits of a redirection entry register
#define IOAPICREDTBL(n)                 (0x10 + (2 * n))

using namespace platform::irq;

/// counter to increment the virtual address for each new IOAPIC
size_t IoApic::numAllocated = 0;

/**
 * Initializes the IOAPIC; we'll map its registers into memory and read out some information.
 */
IoApic::IoApic(const uint64_t _phys, const uint32_t _base) : physBase(_phys), irqBase(_base) {
    int err;
    auto m = vm::Map::kern();

    // map it
    const auto virt = kPlatformRegionMmioIoApic + (numAllocated++ * 0x1000);

    err = m->add(_phys & ~0xFFF, 0x1000, virt, vm::MapMode::kKernelRW | vm::MapMode::MMIO);
    REQUIRE(!err, "failed to map IOAPIC: %d", err);

    this->baseAddr = static_cast<uint32_t>(virt + (_phys & 0xFFF));

    // read some APIC info
    const uint8_t apicVer = this->read(IOAPICVER) & 0xFF;
    this->id = (this->read(IOAPICID) >> 24) & 0xF0;
    this->numIrqs = ((this->read(IOAPICVER) >> 16) & 0x7F) + 1;

    log("APIC ID %u, version %02x, num irqs %u (base %lu)", this->id, apicVer, this->numIrqs, this->irqBase);

    // only map ISA IRQs for the APIC with base 0
    if(this->irqBase == 0) {
        this->mapIsaIrqs();
    }
}

/**
 * Identity maps the 16 ISA interrupts into the first 16 APIC interrupts, as outlined by the ACPI
 * specification. We'll apply overrides to this later.
 */
void IoApic::mapIsaIrqs() {
    // default ISA interrupt flags:
    RedirectionEntry r;
    r.lower = r.upper = 0;

    // send to CPU 0 as a regular interrupt
    r.destination = 0;
    r.destMode = 0; // physical mode: APIC ID

    for(size_t i = 0; i < 16; i++) {
        r.vector = kFirstVector + i;
        this->write(IOAPICREDTBL(i), r.lower);
        this->write(IOAPICREDTBL(i)+1, r.upper);
    }
}


/**
 * Sets a redirection table entry for the given interrupt.
 *
 * The destination value is the index we look up for; the vector value is written to the value of
 * the "bus irq."
 */
void IoApic::remap(const uint8_t irq, const uint32_t dest, const IrqFlags f) {
    const auto idx = dest - this->irqBase;

    // build redirection entry
    RedirectionEntry r;
    r.lower = r.upper = 0;

    r.vector = kFirstVector + irq;

    // currently, all IRQs go to CPU 0
    r.destination = 0;
    r.destMode = 0; // physical mode: APIC ID

    r.pinPolarity = flags(f & IrqFlags::PolarityLow) ? 1 : 0;
    r.triggerMode = flags(f & IrqFlags::TriggerLevel) ? 1 : 0;

    if(flags(f & IrqFlags::TypeNMI)) {
        r.delvMode = 0b100; // NMI
    } else {
        r.delvMode = 0b000; // fixed
    }

    // write to IOAPIC
    this->write(IOAPICREDTBL(idx), r.lower);
    this->write(IOAPICREDTBL(idx)+1, r.upper);

#if LOG_IRQ_REMAP
    log("remapping IOAPIC relative irq %lu (system irq %lu) to %u (%08lx %08lx)", idx, dest, irq,
            r.upper, r.lower);
#endif
}

