#include "Parser.h"

#include "irq/pic.h"
#include "irq/Manager.h"

#include <bootboot.h>
#include <platform.h>

#include <log.h>
#include <string.h>

extern "C" BOOTBOOT bootboot;

using namespace platform;

AcpiParser *AcpiParser::gShared = nullptr;
bool AcpiParser::gLogTables = false;
bool AcpiParser::gLogLapic = false;
bool AcpiParser::gLogIoapic = false;
bool AcpiParser::gLogApicRoutes = false;

/**
 * Allocate the shared ACPI parser.
 */
void AcpiParser::Init() {
    REQUIRE(!gShared, "global acpi parser already initialized");
    gShared = new AcpiParser(bootboot.arch.x86_64.acpi_ptr);
}

/**
 * Initializes a new ACPI parser, given the location of an RSDT/XSDT in physical memory.
 *
 * @param phys Physical address of either an RSDT or XSDT table
 */
AcpiParser::AcpiParser(const uintptr_t phys) : rsdpPhys(phys) {
    // read out the signature of the table
    auto sdt = reinterpret_cast<const SdtHeader *>(phys + kPhysIdentityMap);

    char sdtSig[5] = {0};
    memcpy(sdtSig, sdt->signature, 4);

    // is it an XSDT
    if(!strncmp("XSDT", sdtSig, 4)) {
        auto xsdt = reinterpret_cast<const XSDT *>(sdt);
        auto isValid = xsdt->validateChecksum();
        REQUIRE(isValid, "invalid checksum for ACPI table %s: %x", "XSDT", xsdt->head.checksum);

        // discover each table
        const auto numEntries = (xsdt->head.length - sizeof(xsdt->head)) / 8;

        for(size_t i = 0; i < numEntries; i++) {
            this->foundTable(xsdt->ptrs[i]);
        }
    }
    // TODO: handle RSDT
    // unknown table type
    else {
        panic("invalid ACPI root table signature '%s'", sdtSig);
    }
}

/**
 * Called when a new table has been discovered.
 *
 * Map its header, discover its signature, and if it's a table we're interested in, then create a
 * permanent mapping for the table and all of its data.
 */
void AcpiParser::foundTable(const uintptr_t physAddr) {
    char sdtSig[5] = {0};

    // get the table's header and read its signature
    auto hdr = reinterpret_cast<const SdtHeader *>(physAddr + kPhysIdentityMap);
    memcpy(sdtSig, hdr->signature, 4);

    if(gLogTables) {
        log("ACPI: Found table %s at $%p rev %u", sdtSig, physAddr, hdr->revision);
    }

    // validate checksum
    auto isValid = hdr->validateChecksum();
    REQUIRE(isValid, "invalid checksum for ACPI table %s: %x", sdtSig, hdr->checksum);

    // invoke the appropriate parse handler
    if(!strncmp(hdr->signature, "APIC", 4)) {
        REQUIRE(hdr->length >= sizeof(MADT), "invalid %s table size: %u", sdtSig, hdr->length);
        auto table = reinterpret_cast<const MADT *>(hdr);
        this->parse(table);
    }
    else if(!strncmp(hdr->signature, "HPET", 4)) {
        REQUIRE(hdr->length >= sizeof(HPET), "invalid %s table size: %u", sdtSig, hdr->length);
        auto table = reinterpret_cast<const HPET *>(hdr);
        this->parse(table);
    }
    else {
        // unhandled ACPI table; ignore it
    }
}

/**
 * Handles a HPET table. This doesn't currently do anything other than printing the information
 * contained in the table.
 */
void AcpiParser::parse(const HPET *table) {
    log("HPET rev %d; have %d %d-bit comparators (HPET num %u) min tick %u protection %02x "
            "address %p (addr space %d, reg width %d, offset %d)",
            table->hwRev, table->numComparators, table->counter64 ? 64 : 32, table->hpetNo,
            table->minTick, table->pageProtection,
            table->address.physAddr, (int) table->address.spaceId, table->address.regWidth,
            table->address.regOffset);
}

/**
 * Handles the contents of the MADT (multiple APIC description table) to determine interrupt
 * configuration for the system.
 *
 * At the time the ACPI tables are scanned, only the system global interrupt controllers (the
 * IOAPICs) and their associated interrupt configurations are resolved. The processor-local APICs,
 * and NMI configuration is applied during interrupt manager per-core setup.
 */
void AcpiParser::parse(const MADT *table) {
    this->apicInfo = table;

    // disable legacy PIC if needed
    const bool hasPic = ((table->flags & (1 << 0)));
    if(hasPic) {
        irq::LegacyPic::disable();
    }
    log("Has legacy 8259 PIC? %s", hasPic ? "yes" : "no");

    // loop over each of the tables
    const MADT::RecordHdr *record = table->records;

    for(; ((uintptr_t) record) < ((uintptr_t) table + table->head.length);
            record = (MADT::RecordHdr *) ((uintptr_t) record + record->length)) {
        // per ACPI spec, ignore all > 8
        if(record->type > 8) continue;

        switch(record->type) {
            // processor local APIC
            case 0: {
                REQUIRE(record->length >= sizeof(MADT::LocalApic), "Invalid record length: type %u, length %u", record->type, record->length);
                auto lapic = reinterpret_cast<const MADT::LocalApic *>(record);
                this->madtRecord(table, lapic);
                break;
            }

            // IO APIC
            case 1: {
                REQUIRE(record->length >= sizeof(MADT::IoApic), "Invalid record length: type %u, length %u", record->type, record->length);
                auto ioapic = reinterpret_cast<const MADT::IoApic *>(record);
                this->madtRecord(table, ioapic);
                break;
            }

            // interrupt source definition
            case 2: {
                REQUIRE(record->length >= sizeof(MADT::IrqSourceOverride), "Invalid record length: type %u, length %u", record->type, record->length);
                auto irq = reinterpret_cast<const MADT::IrqSourceOverride *>(record);
                this->madtRecord(table, irq);
                break;
            }

            // Processor local (or may be shared) NMI source
            case 4: {
                REQUIRE(record->length >= sizeof(MADT::Nmi), "Invalid record length: type %u, length %u", record->type, record->length);
                auto irq = reinterpret_cast<const MADT::Nmi *>(record);
                this->madtRecord(table, irq);
                break;
            }
                break;

            default:
                // XXX: should this be a panic?
                panic("Unknown MADT record type %u (length %u)", record->type, record->length);
        }
    }
}

/**
 * An APIC local to a particular processor core (its LAPIC) was discovered.
 */
void AcpiParser::madtRecord(const MADT *table, const MADT::LocalApic *record) {
    const bool enabled = (record->flags & (1 << 0));
    const bool online = (record->flags & (1 << 1));

    if(gLogLapic) {
        log("Detected LAPIC: %p id %x cpu id %x enabled %c online %c", table->lapicAddr,
                record->apicId, record->cpuId, enabled ? 'Y' : 'N', online ? 'Y' : 'N');
    }
}

/**
 * A system-wide IOAPIC (for handling external interrupts) is described in the record.
 */
void AcpiParser::madtRecord(const MADT *, const MADT::IoApic *record) {
    if(gLogIoapic) {
        log("Detected IOAPIC: %p id %x IRQ base %3u", record->ioApicPhysAddr, record->apicId,
                record->irqBase);
    }
}

/**
 * Records an interrupt configuration override.
 *
 * These are used for static interrupt routing, typically for legacy interrupts such as ISA devices
 * that may be emulated by the chipset.
 */
void AcpiParser::madtRecord(const MADT *, const MADT::IrqSourceOverride *record) {
    IrqFlags flags = IrqFlags::None;

    // convert polarity flags
    const auto polarity = (record->flags & 0b11);
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
            if(record->busSource == 0x00) {
                flags |= IrqFlags::PolarityHigh;
            } else {
                panic("Unknown default polarity for bus %02x", record->busSource);
            }
            break;
        // unhandled
        default:
            panic("Unhandled irq polarity: %x", polarity);
    }

    // configure the mapping
    const auto trigger = (record->flags & 0b1100) >> 2;
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
            if(record->busSource == 0x00) {
                flags |= IrqFlags::TriggerEdge;
            } else {
                panic("Unknown trigger mode for bus %02x", record->busSource);
            }
            break;
        // unknown trigger mode
        default:
            panic("Unknown irq trigger mode: %x", trigger);
    }

    if(gLogApicRoutes) {
        log("IRQ override: bus %u irq %u system irq %u flags $%04x", record->busSource, record->irqSource,
                record->systemIrq, flags);
    }
}

/**
 * Specifies configuration for a processor's NMI line.
 */
void AcpiParser::madtRecord(const MADT *, const MADT::Nmi *record) {
    IrqFlags flags = IrqFlags::TypeNMI | IrqFlags::TriggerEdge;

    const auto polarity = (record->flags & 0b11);
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
            // XXX: active high???
            flags |= IrqFlags::PolarityHigh;
            break;
        // unhandled
        default:
            panic("Unhandled NMI polarity: %x", polarity);
    }

    if(gLogApicRoutes) {
        log("APIC NMI: cpu %u, flags $%04x, LINT%d", record->cpuId, record->flags, record->lint);
    }
}
