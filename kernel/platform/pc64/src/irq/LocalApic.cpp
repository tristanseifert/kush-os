#include "LocalApic.h"
#include "ApicRegs.h"

#include "acpi/Parser.h"

#include <stdint.h>

#include <arch/PerCpuInfo.h>
#include <arch/x86_msr.h>
#include <arch/spinlock.h>
#include <log.h>

using namespace platform;

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
 * Initializes a local APIC.
 */
LocalApic::LocalApic(const uintptr_t _id, const uintptr_t cpuId, const uintptr_t phys) : id(_id) {
    this->base = reinterpret_cast<uint32_t *>(phys + kPhysIdentityMap);

    // get APIC version
    const auto version = this->read(kApicRegVersion);

    log("allocated LAPIC %u for cpu %u ($%p) version $%08x base $%p", _id, cpuId, this, version,
            this->base);

    this->configNmi(cpuId);
    this->enable();
}

/**
 * Configures the local APIC's NMI lines.
 */
void LocalApic::configNmi(const uintptr_t cpuId) {
    auto madt = AcpiParser::the()->apicInfo;

    for(const AcpiParser::MADT::RecordHdr *record = madt->records;
            ((uintptr_t) record) < ((uintptr_t) madt + madt->head.length);
            record = (AcpiParser::MADT::RecordHdr *) ((uintptr_t) record + record->length)) {
        // per ACPI spec, ignore all > 8
        if(record->type > 8) continue;

        switch(record->type) {
            /// NMI
            case 4: {
                auto nmi = reinterpret_cast<const AcpiParser::MADT::Nmi *>(record);
                // check ID
                if(nmi->cpuId != cpuId && nmi->cpuId != 0xFF) {
                    break;
                }

                // ID matches, so configure it
                uint32_t value = kVectorNMI;
                REQUIRE((nmi->lint == 0) || (nmi->lint == 1), "Invalid APIC local interrupt %u",
                        nmi->lint);

                // get NMI polarity
                const auto polarity = (nmi->flags & 0b11);
                switch(polarity) {
                    // active low
                    case 0b11:
                        value |= (1 << 13);
                        break;

                    // follows bus specifications: (XXX: is this active high?)
                    case 0b00:
                    // active high
                    case 0b01:
                        break;

                    // unhandled
                    default:
                        panic("Unhandled NMI polarity: %x", polarity);
                }

                // write it to the appropriate register
                value |= (0b100 << 8); // deliver as NMI

                if(nmi->lint == 0) {
                    this->write(kApicRegLvtLint0, value);
                } 
                else if(nmi->lint == 1) {
                    this->write(kApicRegLvtLint1, value);
                }
                break;
            }

            default:
                break;
        }
    }
}

/**
 * Enables the APIC.
 */
void LocalApic::enable() {
    // enable by writing MSR
    const auto base = GetApicBase();
    SetApicBase(base);

    // configure spurious interrupt register
    auto reg = this->read(kApicRegSpurious) & ~0xFF;
    reg |= kVectorSpurious; // spurious vector is 0xFF
    reg |= (1 << 8); // set enable bit
    this->write(kApicRegSpurious, reg);
}

/**
 * Clear the APIC enable bit and uninstall all interrupts.
 */
LocalApic::~LocalApic() {
    // clear enable bit
    auto reg = this->read(kApicRegSpurious);
    reg &= ~(1 << 8);
    this->write(kApicRegSpurious, reg);
}



/**
 * Update the APIC task priority register.
 */
void LocalApic::updateTpr(const Irql irql) {
    uint32_t priority = 0;

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

    //log("%p tpr: %02x", this, priority);
    this->write(kApicRegTaskPriority, priority);
}



/**
 * Returns the current core's local APIC.
 */
LocalApic *LocalApic::the() {
    auto gs = arch::PerCpuInfo::get();
    if(!gs) return nullptr;
    return gs->p.lapic;
}
