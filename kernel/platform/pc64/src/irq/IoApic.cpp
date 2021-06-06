#include "IoApic.h"
#include "../memmap.h"

#include <sched/Task.h>
#include <vm/Map.h>
#include <vm/MapEntry.h>

#include <log.h>

using namespace platform;

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

bool IoApic::gLogInit   = false;
bool IoApic::gLogSet    = false;

/**
 * Creates and initializes a new IOAPIC controller.
 */
IoApic::IoApic(const uintptr_t base, const uint32_t _irqBase, const uint8_t _id) : physBase(base),
    irqBase(_irqBase), id(_id) {
    int err;

    // map the controller
    this->vm = vm::MapEntry::makePhys(this->physBase, arch_page_size(),
            vm::MappingFlags::Read | vm::MappingFlags::Write | vm::MappingFlags::MMIO, true);
    REQUIRE(this->vm, "failed to create %s phys map", "IOAPIC");

    auto map = vm::Map::kern();
    err = map->add(this->vm, sched::Task::kern(), 0, vm::MappingFlags::None, 0, kPlatformRegionMmio,
            (kPlatformRegionMmio + kPlatformRegionMmioLen - 1));
    REQUIRE(!err, "failed to map %s: %d", "IOAPIC", err);

    auto vmBase = map->getRegionBase(this->vm);
    REQUIRE(vmBase, "failed to get %s base address", "IOAPIC");

    this->base = reinterpret_cast<void *>(vmBase);

    // read some APIC info
    const uint8_t apicVer = this->read(IOAPICVER) & 0xFF;
    this->id = (this->read(IOAPICID) >> 24) & 0xF0;
    this->numIrqs = ((this->read(IOAPICVER) >> 16) & 0x7F) + 1;

    if(gLogInit) log("IOAPIC ID %lu, version $%02x; have %lu IRQs (base %lu)", this->id,
            apicVer, this->numIrqs, this->irqBase);

    // only map ISA IRQs for the APIC with base 0
    if(this->irqBase == 0) {
        this->mapIsaIrqs();
    }

    // install overrides (from ACPI tables)
    this->installOverrides();
}

/**
 * Disables the IOAPIC and removes its memory mapping.
 */
IoApic::~IoApic() {
    int err;

    // disable IOAPIC

    // remove VM mapping
    auto map = vm::Map::kern();
    err = map->remove(this->vm, sched::Task::kern());
    REQUIRE(!err, "failed to unmap %s phys map", "IOAPIC");
}

/**
 * Identity maps the 16 ISA interrupts into the first 16 APIC interrupts, as outlined by the ACPI
 * specification. We'll apply overrides to this later.
 */
void IoApic::mapIsaIrqs() {
    // default ISA interrupt flags:
    RedirectionEntry r;
    r.lower = r.upper = 0;
    r.mask = 1;

    // send to CPU 0 as a regular interrupt
    r.destination = 0;
    r.destMode = 0; // physical mode: APIC ID

    for(size_t i = 0; i < 16; i++) {
        r.vector = kFirstVector + i;
        this->setRedirEntry(i, r);
    }
}

/**
 * Installs all IRQ overrides from the ACPI tables.
 */
void IoApic::installOverrides() {
    auto madt = AcpiParser::the()->apicInfo;

    for(const AcpiParser::MADT::RecordHdr *record = madt->records;
            ((uintptr_t) record) < ((uintptr_t) madt + madt->head.length);
            record = (AcpiParser::MADT::RecordHdr *) ((uintptr_t) record + record->length)) {
        // per ACPI spec, ignore all > 8
        if(record->type > 8) continue;

        switch(record->type) {
            // Describes an interrupt definition
            case 2: {
                auto info = reinterpret_cast<const AcpiParser::MADT::IrqSourceOverride *>(record);
                if(this->handlesIrq(info->systemIrq)) {
                    this->addOverride(info);
                }
                break;
            }

            default:
                break;
        }
    }
}

/**
 * Process an IRQ override from ACPI.
 */
void IoApic::addOverride(const AcpiParser::MADT::IrqSourceOverride *info) {
    IrqFlags flags = IrqFlags::None;

    // convert polarity flags
    const auto polarity = (info->flags & 0b11);
    switch(polarity) {
        // active high
        case 0b01:
            flags |= IrqFlags::PolarityHigh;
            break;
        // active low
        case 0b11:
            flags |= IrqFlags::PolarityLow;
            break;
        // follows bus specifications
        case 0b00:
            // ISA is active high
            if(info->busSource == 0x00) {
                flags |= IrqFlags::PolarityHigh;
            } else {
                panic("Unknown default polarity for bus %02x", info->busSource);
            }
            break;
        // unhandled
        default:
            panic("Unhandled irq polarity: %x", polarity);
    }

    // configure the mapping
    const auto trigger = (info->flags & 0b1100) >> 2;
    switch(trigger) {
        // edge triggered
        case 0b01:
            flags |= IrqFlags::TriggerEdge;
            break;
        // level triggered
        case 0b11:
            flags |= IrqFlags::TriggerLevel;
            break;
        // follows bus specifications
        case 0b00:
            // ISA is edge triggered
            if(info->busSource == 0x00) {
                flags |= IrqFlags::TriggerEdge;
            } else {
                panic("Unknown trigger mode for bus %02x", info->busSource);
            }
            break;
        // unknown trigger mode
        default:
            panic("Unknown irq trigger mode: %x", trigger);
    }

    // add it
    this->remap(info->irqSource, info->systemIrq, flags);
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

    // IRQs are masked by default
    r.mask = 1;

    // currently, all IRQs go to CPU 0
    r.destination = 0;
    r.destMode = 0; // physical mode: APIC ID

    r.pinPolarity = TestFlags(f & IrqFlags::PolarityLow) ? 1 : 0;
    r.triggerMode = TestFlags(f & IrqFlags::TriggerLevel) ? 1 : 0;

    if(TestFlags(f & IrqFlags::TypeNMI)) {
        r.delvMode = 0b100; // NMI
    } else {
        r.delvMode = 0b000; // fixed
    }

    // write to IOAPIC
    this->setRedirEntry(idx, r);

    if(gLogSet) log("remapping IOAPIC relative irq %lu (system irq %lu) to %u (%08lx %08lx) "
            "active %s, %s triggerd", idx, dest, irq, r.upper, r.lower,  
            r.pinPolarity ? "low" : "high", r.triggerMode ? "level" : "edge");
}

/**
 * Sets the mask state of the given interrupt.
 */
void IoApic::setIrqMasked(const uint8_t irq, const bool masked) {
    // read the redirection entry
    const auto idx = irq - this->irqBase;
    auto r = this->getRedirEntry(idx);

    // write it back wit hthe right bit set
    r.mask = masked ? 1 : 0;
    this->setRedirEntry(idx, r);
}



/**
 * Reads an irq redirection entry.
 */
const IoApic::RedirectionEntry IoApic::getRedirEntry(const size_t offset) {
    RedirectionEntry r;

    r.lower = this->read(IOAPICREDTBL(offset));
    r.upper = this->read(IOAPICREDTBL(offset)+1);

    return r;
}

/**
 * Writes the redirection entry to the given interrupt index.
 */
void IoApic::setRedirEntry(const size_t offset, const RedirectionEntry &entry) {
    this->write(IOAPICREDTBL(offset), entry.lower);
    this->write(IOAPICREDTBL(offset)+1, entry.upper);
}

