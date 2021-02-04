#include "Manager.h"

#include "multiboot2.h"

#include <mem/PhysicalAllocator.h>
#include <vm/Map.h>

#include <new>
#include <log.h>
#include <string.h>

using namespace platform::acpi;

static char gSharedBuf[sizeof(Manager)];
Manager *Manager::gShared = (Manager *) &gSharedBuf;


/**
 * Initializes the ACPI handler using Multiboot-provided old style RSDP.
 */
void Manager::init(struct multiboot_tag_old_acpi *info) {
    int err;

    // read out the RSDP
    REQUIRE(info->size >= sizeof(RSDPv1), "invalid RSDP size (%d bytes)", info->size);
    const auto rsdp = reinterpret_cast<RSDPv1 *>(info->rsdp);

    // validate signature
    err = strncmp(rsdp->signature, "RSD PTR ", 8);
    REQUIRE(!err, "invalid RSDP signature: '%8s'", rsdp->signature);

    // then, validate the checksum
    uint32_t sum = 0;
    for(size_t i = 0; i < sizeof(RSDPv1); i++) {
        sum += ((uint8_t *) rsdp)[i];
    }
    REQUIRE((sum & 0xFF) == 0, "invalid checksum result: %08lx", sum);

    // initialize it
    log("RSDP revision: %d (OEM id '%6s')", rsdp->revision, rsdp->OEMID);

    new(gShared) Manager(rsdp->rsdtPhysAddr);
}



/**
 * Initializes the ACPI table parser. This just stores the physical addresses away and clears some
 * state; we'll map the tables later when VM is available.
 */
Manager::Manager(const uint64_t _rsdtPhys) : rsdtPhys(_rsdtPhys) {

}



/**
 * Maps the RSDT and parses it for all the information we need.
 */
void Manager::parseTables() {
    int err;

    auto m = vm::Map::kern();

    // map the RSDT and calculate checksum
    const uint32_t rsdtVirt = 0xF0000000;

    log("Mapping RSDT %016llx to %08lx", this->rsdtPhys, rsdtVirt);
    err = m->add(this->rsdtPhys & ~0xFFF, 0x1000, rsdtVirt, vm::MapMode::kKernelRead);
    REQUIRE(!err, "failed to map RSDT: %d", err);

    mem::PhysicalAllocator::reserve(this->rsdtPhys & ~0xFFF);

    this->rsdt = reinterpret_cast<RSDT *>(rsdtVirt + (this->rsdtPhys & 0xFFF));

    REQUIRE(this->rsdt->validateChecksum(), "invalid RSDT checksum");

    // look at each table
    log("RSDT %p: signature '%4s' length %lu (OEM ID '%6s' rev '%8s')", this->rsdt,
            this->rsdt->head.signature,
            this->rsdt->head.length, this->rsdt->head.OEMID, this->rsdt->head.OEMTableID);

    const size_t numTables = (this->rsdt->head.length - sizeof(SdtHeader)) / sizeof(uint32_t);
    for(size_t i = 0; i < numTables; i++) {
        SdtHeader *tableHeader = nullptr;
        const auto addr = this->rsdt->ptrs[i];

        // the table is in the same page as we've already mapped
        if((addr & ~0xFFF) == (this->rsdtPhys & ~0xFFF)) {
            tableHeader = reinterpret_cast<SdtHeader *>(rsdtVirt + (addr & 0xFFF));
        }
        // we need to map a separate page for this table
        else {
            // TODO: implement
            mem::PhysicalAllocator::reserve(addr & ~0xFFF);
            panic("mapping extra SDTs is not yet supported (phys %08lx)", addr);
        }

        // invoke the appropriate handler
        if(!strncmp(tableHeader->signature, "APIC", 4)) {
            REQUIRE(tableHeader->length >= sizeof(MADT), "invalid table size: %lu", tableHeader -> length);
            auto table = reinterpret_cast<const MADT *>(tableHeader);
            this->parse(table);
        }
        else if(!strncmp(tableHeader->signature, "HPET", 4)) {
            REQUIRE(tableHeader->length >= sizeof(HPET), "invalid table size: %lu", tableHeader -> length);
            auto table = reinterpret_cast<const HPET *>(tableHeader);
            this->parse(table);
        }
        else {
            char name[5];
            memcpy(&name, tableHeader->signature, 4);

            log("unhandled table %lu: %08lx (type '%4s', length %lu)", i, addr, name, tableHeader->length);
        }
    }
}



/**
 * Parses the MADT, identified by the signature 'APIC'.
 */
void Manager::parse(const MADT *table) {
    log("LAPIC addr: %08lx, flags %08lx", table->lapicAddr, table->flags);

    // loop over each of the tables
    const MADT::RecordHdr *record = table->records;

    for(; ((uintptr_t) record) < ((uintptr_t) table + table->head.length);
            record = (MADT::RecordHdr *) ((uintptr_t) record + record->length)) {
        switch(record->type) {
            // processor local APIC
            case 0: {
                REQUIRE(record->length >= sizeof(MADT::LocalApic), "Invalid record length: type %u, length %u", record->type, record->length);
                auto lapic = reinterpret_cast<const MADT::LocalApic *>(record);
                this->madtRecord(lapic);
                break;
            }

            // IO APIC
            case 1: {
                REQUIRE(record->length >= sizeof(MADT::IoApic), "Invalid record length: type %u, length %u", record->type, record->length);
                auto ioapic = reinterpret_cast<const MADT::IoApic *>(record);
                this->madtRecord(ioapic);
                break;
            }

            // interrupt source definition
            case 2: {
                REQUIRE(record->length >= sizeof(MADT::IrqSourceOverride), "Invalid record length: type %u, length %u", record->type, record->length);
                auto irq = reinterpret_cast<const MADT::IrqSourceOverride *>(record);
                this->madtRecord(irq);
                break;
            }

            // Processor local (or may be shared) NMI source
            case 4: {
                REQUIRE(record->length >= sizeof(MADT::Nmi), "Invalid record length: type %u, length %u", record->type, record->length);
                auto irq = reinterpret_cast<const MADT::Nmi *>(record);
                this->madtRecord(irq);
                break;
            }
                break;

            default:
                panic("Unknown MADT record type %u (length %u)", record->type, record->length);
        }
    }
}

/**
 * Handles the MADT processor-local APIC record.
 */
void Manager::madtRecord(const MADT::LocalApic *record) {
    log("Local APIC for processor %u -> %u; flags %08x", record->cpuId, record->apicId,
            record->flags);
}

/**
 * Handles the MADT global IO APIC record.
 */
void Manager::madtRecord(const MADT::IoApic *record) {
    log("Global IO APIC %u at %08lx (base %08lx)", record->apicId, record->ioApicPhysAddr,
            record->irqBase);
}

/**
 * Configures a particular interrupt.
 */
void Manager::madtRecord(const MADT::IrqSourceOverride *record) {
    log("APIC IRQ: source (bus %02x, irq %02x) -> %08lx (flags %04x)", record->busSource,
            record->irqSource, record->systemIrq, record->flags);
}

/**
 * Configures the non-maskable interrupt vector for a processor.
 */
void Manager::madtRecord(const MADT::Nmi *record) {
    log("APIC NMI: processor %u, flags %04x, LINT%d", record->cpuId, record->flags, record->lint);
}



/**
 * Parses the HPET table.
 */
void Manager::parse(const HPET *table) {
    log("HPET rev %d; have %d %d-bit comparators (HPET num %u) min tick %u protection %02x "
            "address %llx (addr space %d, reg width %d, offset %d)",
            table->hwRev, table->numComparators, table->counter64 ? 64 : 32, table->hpetNo,
            table->minTick, table->pageProtection,
            table->address.physAddr, (int) table->address.spaceId, table->address.regWidth,
            table->address.regOffset);
}
