#include "Manager.h"
#include "LocalApic.h"
#include "IoApic.h"

#include "acpi/Parser.h"

#include <arch/PerCpuInfo.h>

#include <new>
#include <cpuid.h>
#include <stdint.h>

#include <log.h>

using namespace platform;

static uint8_t gSharedBuf[sizeof(IrqManager)] __attribute__((aligned(alignof(IrqManager))));
IrqManager *IrqManager::gShared = nullptr;

/**
 * Initializes the shared irq manager.
 */
void IrqManager::Init() {
    REQUIRE(!gShared, "cannot re-initialize irq manager");

    gShared = reinterpret_cast<IrqManager *>(&gSharedBuf);
    new (gSharedBuf) IrqManager;
}

/**
 * Initialize all IOAPICs listed in the system's ACPI tables.
 */
void IrqManager::InitSystemControllers() {
    REQUIRE(gShared, "IrqManager not initialized");

    // find all IOAPIC entries
    auto madt = AcpiParser::the()->apicInfo;

    for(const AcpiParser::MADT::RecordHdr *record = madt->records;
            ((uintptr_t) record) < ((uintptr_t) madt + madt->head.length);
            record = (AcpiParser::MADT::RecordHdr *) ((uintptr_t) record + record->length)) {
        // per ACPI spec, ignore all > 8
        if(record->type > 8) continue;

        switch(record->type) {
            // Global IOAPIC
            case 1:
                gShared->initIoApic(reinterpret_cast<const AcpiParser::MADT::IoApic *>(record));
                break;

            default:
                break;
        }
    }
}

/**
 * Initializes the local APIC for the current core.
 */
void IrqManager::InitCoreLocalController() {
    REQUIRE(gShared, "IrqManager not initialized");

    // get local APIC ID for this processor
    uint32_t ebx, unused1, unused2, unused3;
    __get_cpuid(0x01, &unused1, &ebx, &unused2, &unused3);

    const auto id = (ebx & 0xFF000000) >> 24;

    // get the LAPIC base address
    auto madt = AcpiParser::the()->apicInfo;
    uintptr_t lapicPhys = madt->lapicAddr;

    // scan the MADT for an IOAPIC for this core
    for(const AcpiParser::MADT::RecordHdr *record = madt->records;
            ((uintptr_t) record) < ((uintptr_t) madt + madt->head.length);
            record = (AcpiParser::MADT::RecordHdr *) ((uintptr_t) record + record->length)) {
        // per ACPI spec, ignore all > 8
        if(record->type > 8) continue;

        switch(record->type) {
            // processor local APIC
            case 0: {
                // check ID
                auto info = reinterpret_cast<const AcpiParser::MADT::LocalApic *>(record);
                if(info->apicId != id)
                    break;
                // ID matches, so configure it
                return gShared->initLapic(lapicPhys, id, info);
            }

            default:
                break;
        }
    }


    // if we get here, no IOAPIC for core :(
    panic("no LAPIC for processor %u", id);
}

/**
 * Initializes an IOAPIC shared among all processors in the system.
 *
 * @param record Pointer to the IOAPIC ACPI record
 */
void IrqManager::initIoApic(const AcpiParser::MADT::IoApic *info) {
    auto ioapic = new IoApic(info->ioApicPhysAddr, info->irqBase, info->apicId);
    this->ioapics.push_back(ioapic);
}

/**
 * Sets up a local APIC for the processor, given a MADT LAPIC record.
 *
 * @note You must be executing on the core whose APIC ID you specify.
 *
 * @param lapicPhys Physical address of the local APIC registers
 * @param cpu Local APIC ID of the processor
 * @param record Pointer to an ACPI MADT record, which is the LocalApic type
 */
void IrqManager::initLapic(const uintptr_t lapicPhys, const uintptr_t cpu,
        const AcpiParser::MADT::LocalApic *info) {
    // create the local APIC
    auto lapic = new LocalApic(info->apicId, info->cpuId, lapicPhys);

    // store it
    auto gs = arch::PerCpuInfo::get();
    gs->p.lapic = lapic;
}


/**
 * Sets the mask state of the given system interrupt.
 *
 * @param vector System interrupt number to mask/unmask
 * @param isMasked Whether the interrupt is to be masked or not.
 */
void IrqManager::setMasked(const uintptr_t vector, const bool isMasked) {
    // vectors less than 256 correspond to a physical interrupt
    if(vector < 0x100) {
        // see if an IOAPIC handles it
        for(auto ioapic : this->ioapics) {
            if(!ioapic->handlesIrq(vector)) continue;

            ioapic->setIrqMasked(vector, isMasked);
            return;
        }
    }

    // nobody handled this vector
    panic("don't know how to %s irq %x", "mask", vector);
}
