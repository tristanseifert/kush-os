#include "Manager.h"
#include "Apic.h"
#include "IoApic.h"
#include "pic.h"

#include "memmap.h"

#include <arch/idt.h>
#include <arch/gdt.h>
#include <vm/Map.h>

#include <new>
#include <log.h>

using namespace platform::irq;

static char gSharedBuf[sizeof(Manager)];
Manager *Manager::gShared = nullptr;

// external ISRs
extern "C" void platform_isr_spurious_pic();
extern "C" void platform_isr_spurious_pic2();
extern "C" void platform_isr_spurious_apic();

extern "C" void platform_isr_isa_0();
extern "C" void platform_isr_isa_1();
extern "C" void platform_isr_isa_2();
extern "C" void platform_isr_isa_3();
extern "C" void platform_isr_isa_4();
extern "C" void platform_isr_isa_5();
extern "C" void platform_isr_isa_6();
extern "C" void platform_isr_isa_7();
extern "C" void platform_isr_isa_8();
extern "C" void platform_isr_isa_9();
extern "C" void platform_isr_isa_10();
extern "C" void platform_isr_isa_11();
extern "C" void platform_isr_isa_12();
extern "C" void platform_isr_isa_13();
extern "C" void platform_isr_isa_14();
extern "C" void platform_isr_isa_15();

/**
 * Initializes the shared IRQ manager.
 */
void Manager::init() {
    gShared = (Manager *) &gSharedBuf;
    new(gShared) Manager();
}

/**
 * Performs the IRQ mapping.
 */
void Manager::setupIrqs() {
    gShared->configure();
    gShared->configureOverrides();
    gShared->installHandlers();
    gShared->enable();
}

/**
 * Sets up a new interrupt manager.
 */
Manager::Manager() {

}

/**
 * Tears down the manager by deallocating all APIC structures.
 */
Manager::~Manager() {
    for(auto apic : this->apics) {
        delete apic;
    }
    for(auto ioapic : this->ioapics) {
        delete ioapic;
    }
}



/**
 * Sets whether this machine has a legacy PIC. If so, we'll have to disable and remap them.
 */
void Manager::setHasLegacyPic(const bool yes) {
    this->has8259 = yes;
}

/**
 * Handles a detected local APIC. We'll create an appropriate instance and add it to our list.
 */
void Manager::detectedLapic(const uint64_t phys, const uint8_t id, const uint8_t cpu,
        const bool enabled, const bool onlineable) {
    int err;

    // perform virtual mapping if needed
    if(!this->apicVirtBase) {
        auto m = vm::Map::kern();

        err = m->add(phys & ~0xFFF, 0x1000, kPlatformRegionMmioApic, vm::MapMode::kKernelRW | vm::MapMode::MMIO);
        REQUIRE(!err, "failed to map APIC: %d", err);

        this->apicVirtBase = reinterpret_cast<uint32_t *>((kPlatformRegionMmioApic | (phys & 0xFFF)));

        this->apicPhysBase = phys;
    }

    REQUIRE(this->apicPhysBase == phys, "invalid APIC address %016llx; previous map was %016llx",
            phys, this->apicPhysBase);

    // allocate apic
    auto apic = new Apic(this->apicVirtBase, cpu, id, enabled, onlineable);
    this->apics.push_back(apic);
}

/**
 * Handles a detected IO APIC.
 */
void Manager::detectedIoapic(const uint64_t phys, const uint8_t cpu, const uint32_t irqBase) {
    auto ioapic = new IoApic(phys, irqBase);
    this->ioapics.push_back(ioapic);

    log("IOAPIC %p handles processor(s) %02x", ioapic, cpu);
}

/**
 * Handles an IRQ override as detected in the ACPI tables.
 */
void Manager::detectedOverride(const uint8_t bus, const uint8_t source, const uint32_t globalIrq, const IrqFlags flags) {
    // we only support EISA bus
    REQUIRE(bus == 0x00, "invalid irq override bus: $%02x", bus);

    // store the mapping
    //log("IRQ override: source (%u, %u) -> %lu (flags %04lx)", bus, source, globalIrq, (uint32_t) flags);

    IrqOverride o;
    o.bus = bus;
    o.busIrq = source;
    o.irqNo = globalIrq;
    o.flags = flags;

    this->overrides.push_back(o);
}

/**
 * Handles a detected NMI override.
 *
 * In these cases, the "bus irq" field actually means the LINT# that the interrupt arrives on; the
 * global IRQ number is redefined as the CPU number, or 0xFF for all.
 */
void Manager::detectedNmi(const uint8_t cpus, const uint8_t lint, const IrqFlags flags) {
    IrqOverride o;
    o.bus = 0xFF;
    o.busIrq = lint;
    o.irqNo = cpus;
    o.flags = flags;

    this->overrides.push_back(o);
}



/**
 * Configures all APIC and IOAPIC registers.
 */
void Manager::configure() {

}

/**
 * Configures APIC interrupt overrides, if any.
 */
void Manager::configureOverrides() {
    for(auto &o : this->overrides) {
        bool remapped = false;

        // handle interrupts (i.e. not NMIs)
        if(!flags(o.flags & irq::IrqFlags::TypeNMI)) {
            // check each IOAPIC to see if it handles this irq
            for(auto ioapic : this->ioapics) {
                // it does, so remap it there
                if(ioapic->handlesIrq(o.irqNo)) {
                    ioapic->remap(o.busIrq, o.irqNo, o.flags);
                    remapped = true;
                }
            }

            REQUIRE(remapped, "failed to find IOAPIC to remap (%u, %u) -> %lu", o.bus, o.busIrq,
                    o.irqNo);
        }
        // handle NMIs
        else {
            for(auto apic : this->apics) {
                // match on APIC ID or wildcard
                const uint8_t cpu = o.irqNo;
                if(cpu == 0xFF || cpu == apic->getProcessor()) {
                    apic->mapNmi(o.busIrq, o.flags);
                    remapped = true;
                }
            }

            REQUIRE(remapped, "failed to find APIC to map NMI LIDT%u CPU %02lx", o.busIrq, o.irqNo);
        }
    }
}

/**
 * Installs x86 interrupt handlers for all vectors covered by the APICs.
 */
void Manager::installHandlers() {
    // install the APIC spurious ISR
    idt_set_entry(0xFF, (uint32_t) platform_isr_spurious_apic, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);

    // install the legacy ISA handlers (first 16 IOAPIC vectors)
    const static uintptr_t kIsaIsrs[16] = {
        (uintptr_t) platform_isr_isa_0,  (uintptr_t) platform_isr_isa_1,
        (uintptr_t) platform_isr_isa_2,  (uintptr_t) platform_isr_isa_3,
        (uintptr_t) platform_isr_isa_4,  (uintptr_t) platform_isr_isa_5,
        (uintptr_t) platform_isr_isa_6,  (uintptr_t) platform_isr_isa_7,
        (uintptr_t) platform_isr_isa_8,  (uintptr_t) platform_isr_isa_9,
        (uintptr_t) platform_isr_isa_10, (uintptr_t) platform_isr_isa_11,
        (uintptr_t) platform_isr_isa_12, (uintptr_t) platform_isr_isa_13,
        (uintptr_t) platform_isr_isa_14, (uintptr_t) platform_isr_isa_15,
    };

    for(size_t i = 0; i < 16; i++) {
        idt_set_entry(IoApic::kFirstVector + i, (uint32_t) kIsaIsrs[i], GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);
    }
}

/**
 * Enables interrupts via the APICs.
 *
 * If a legacy PIC is disabled, we still need to contend with the possibility of spurious IRQs;
 * these are the PIC-relative interrupt vectors 7 and 15, or 0x27/0x2F since we remapped them to
 * vector base 0x20 at the start of this process.
 */
void Manager::enable() {
    // disable legacy PICs
    if(this->has8259) {
        log("Disabling legacy 8259 PIC");
        pic_irq_disable();

        //idt_set_entry(0x20, (uint32_t) platform_isr_spurious_pic2, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);

        idt_set_entry(0x27, (uint32_t) platform_isr_spurious_pic, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);
        idt_set_entry(0x2F, (uint32_t) platform_isr_spurious_pic2, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);
    }

    // enable APICs
    for(auto apic : this->apics) {
        apic->enable();
    }
}



/**
 * Handles and routes an ISR appropriately.
 *
 * This function is invoked only for ISRs that require routing; things like spurious interrupts or
 * NMIs have their own handlers. Type codes correspond to values defined in `handlers.h`
 */
void Manager::handleIsr(const uint32_t type) {
    // legacy ISA interrupts?
    if(type >= ISR_ISA_0 && type <= ISR_ISA_15) {
        const auto isaIrqNo = type - ISR_ISA_0;
        log("ISA interrupt %lu", isaIrqNo);

        // acknowledge on first APIC (XXX: fix for SMP)
        this->apics[0]->endOfInterrupt();
    }
}
