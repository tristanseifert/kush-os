#include "LocalApic.h"
#include "ApicRegs.h"

#include "../memmap.h"
#include "acpi/Parser.h"

#include <stdint.h>

#include <sched/Task.h>
#include <sched/Scheduler.h>
#include <vm/Map.h>
#include <vm/MapEntry.h>

#include <arch/IrqRegistry.h>
#include <arch/PerCpuInfo.h>
#include <arch/x86_msr.h>
#include <arch/spinlock.h>
#include <log.h>

using namespace platform;

bool LocalApic::gLogInit = true;
bool LocalApic::gLogRegIo = false;
bool LocalApic::gLogSpurious = true;

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

/// Spurious IRQ trampoline
void platform::ApicSpuriousIrq(const uintptr_t vector, void *ctx) {
    reinterpret_cast<LocalApic *>(ctx)->irqSpurious();
}
/// Scheduler IPI trampoline
void platform::ApicSchedulerIpi(const uintptr_t vector, void *ctx) {
    sched::Scheduler::get()->handleIpi([](void *ctx) {
        auto apic = reinterpret_cast<LocalApic *>(ctx);
        apic->eoi();
    }, ctx);
}

/**
 * Initializes a local APIC.
 */
LocalApic::LocalApic(const uintptr_t _id, const uintptr_t cpuId, const uintptr_t phys) : id(_id) {
    int err;

    // map it somewhere in the architecture VM space
    this->vmEnt = vm::MapEntry::makePhys(phys, arch_page_size(),
            vm::MappingFlags::Read | vm::MappingFlags::Write | vm::MappingFlags::MMIO, true);
    REQUIRE(this->vmEnt, "failed to create %s phys map", "LAPIC");

    auto map = vm::Map::kern();
    err = map->add(this->vmEnt, sched::Task::kern(), 0, vm::MappingFlags::None, 0, kPlatformRegionMmio,
            (kPlatformRegionMmio + kPlatformRegionMmioLen - 1));
    REQUIRE(!err, "failed to map %s: %d", "LAPIC", err);

    auto base = map->getRegionBase(this->vmEnt);
    REQUIRE(base, "failed to get %s base address", "LAPIC");

    this->base = reinterpret_cast<uint32_t *>(base);

    // get APIC version
    const auto version = this->read(kApicRegVersion);

    if(gLogInit) {
        log("allocated LAPIC %u for cpu %u ($%p) version $%08x base $%p (phys $%p)", _id, cpuId,
                this, version, this->base, phys);
    }

    this->configNmi(cpuId);

    // install amd64 irq handlers and enable APIC
    auto irq = arch::IrqRegistry::the();
    irq->install(kVectorSpurious, ApicSpuriousIrq, this);

    irq->install(kVectorSchedulerIpi, ApicSchedulerIpi, this);

    this->enable();

    // once the APIC is enabled, we can set up the timer
    this->timer = new ApicTimer(this);
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

                // ID matches, so configure it (vector number ignored)
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
    int err;

    // turn off timer
    delete this->timer;

    // clear enable bit
    auto reg = this->read(kApicRegSpurious);
    reg &= ~(1 << 8);
    this->write(kApicRegSpurious, reg);

    // remove mapping
    auto vm = vm::Map::kern();
    err = vm->remove(this->vmEnt, sched::Task::kern());
    REQUIRE(!err, "failed to unmap %s phys map", "LAPIC");

    // remove irq handlers
    auto irq = arch::PerCpuInfo::get()->irqRegistry;
    irq->remove(kVectorSpurious);
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
 * Handle a received spurious IRQ. This just increments a counter.
 */
void LocalApic::irqSpurious() {
    this->numSpurious++;

    if(gLogSpurious) {
        log("APIC %3u: spurious irq", this->id);
    }
}

/**
 * Acknowledges an interrupt.
 */
void LocalApic::eoi() {
    this->write(kApicRegEndOfInt, 0);
}



/**
 * Returns the current core's local APIC.
 */
LocalApic *LocalApic::the() {
    auto gs = arch::PerCpuInfo::get();
    if(!gs) return nullptr;
    return gs->p.lapic;
}

/**
 * Sends a self IPI.
 */
void LocalApic::selfIpi(const uint8_t vector) {
    this->write(kApicRegInterruptCmdLow, 
            (0b01 << 18) | // destination shorthand = self
               (1 << 14) | // level = 1
            (0b000 << 8) | // fixed delivery
            vector);
}

/**
 * Sends an IPI to a remote APIC.
 */
void LocalApic::remoteIpi(const uintptr_t coreId, const uint8_t vector) {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}




/**
 * Configure the core local timer, used by the scheduler code.
 */
void platform::SetLocalTimer(const uint64_t interval, const bool repeat) {
    auto timer = LocalApic::theTimer();
    REQUIRE(timer, "invalid %s", "LAPIC timer");

    timer->setInterval(interval, repeat);
}

/**
 * Stops the local APIC timer.
 */
void platform::StopLocalTimer() {
    auto timer = LocalApic::theTimer();
    REQUIRE(timer, "invalid %s", "LAPIC timer");

    timer->stop();
}

/**
 * Sends a scheduler self IPI to the current core.
 */
void platform::RequestSchedulerIpi() {
    LocalApic::the()->selfIpi(LocalApic::kVectorSchedulerIpi);
}

/**
 * Sends a scheduler IPI to the given core.
 */
void platform::RequestSchedulerIpi(const uintptr_t coreId) {
    LocalApic::the()->remoteIpi(coreId, LocalApic::kVectorSchedulerIpi);
}
