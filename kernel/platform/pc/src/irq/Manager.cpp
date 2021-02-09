#include "Manager.h"
#include "Apic.h"
#include "IoApic.h"
#include "pic.h"

#include "timer/Manager.h"
#include "timer/LocalApicTimer.h"

#include "memmap.h"

#include <arch/idt.h>
#include <arch/gdt.h>
#include <vm/Map.h>

#include <new>
#include <log.h>

using namespace platform::irq;

static char gSharedBuf[sizeof(Manager)] __attribute__((aligned(64)));
Manager *Manager::gShared = nullptr;

// external ISRs
extern "C" void platform_isr_pic_spurious_pri();
extern "C" void platform_isr_pic_spurious_sec();
extern "C" void platform_isr_apic_spurious();
extern "C" void platform_isr_apic_timer();
extern "C" void platform_isr_apic_dispatch();

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

    gShared->setupTimebase();
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

            REQUIRE(remapped, "failed to find IOAPIC to remap (%u, %u) -> %u", o.bus, o.busIrq,
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

            REQUIRE(remapped, "failed to find APIC to map NMI LIDT%u CPU %02x", o.busIrq, o.irqNo);
        }
    }
}

/**
 * Installs x86 interrupt handlers for all vectors covered by the APICs.
 */
void Manager::installHandlers() {
    // install the APIC spurious and timer ISR
    idt_set_entry(0xFF, (uint32_t) platform_isr_apic_spurious, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);
    idt_set_entry(timer::LocalApicTimer::kTimerVector, (uint32_t) platform_isr_apic_timer,
            GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);

    // register for the scheduler dispatch IPIs
    idt_set_entry(Apic::kVectorDispatch, (uint32_t) platform_isr_apic_dispatch, GDT_KERN_CODE_SEG,
            IDT_FLAGS_ISR);

    const auto token = this->addHandler(ISR_APIC_DISPATCH_IPI, [](void *ctx, const uint32_t type) {
        platform_raise_irql(Irql::Scheduler);
        platform_kern_scheduler_update(type);
        platform_lower_irql(Irql::Passive);

        return true;
    }, this);
    REQUIRE(token, "failed to register dispatch IPI");

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

        idt_set_entry(0x27, (uint32_t) platform_isr_pic_spurious_pri, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);
        idt_set_entry(0x2F, (uint32_t) platform_isr_pic_spurious_sec, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR);
    }

    // enable APICs
    for(auto apic : this->apics) {
        apic->enable();
    }
}

/**
 * Configures all APIC-local timers to act as system timebases.
 */
void Manager::setupTimebase() {
    // TODO: fix this for SMP
    for(auto apic : this->apics) {
        auto timer = apic->getTimer();
        if(!timer) continue;

        timer->setInterval(kTimebaseInterval);

        timer::Manager::gShared->timebase = timer;

        // register an interrupt handler as well
        const auto token = this->addHandler(ISR_APIC_TIMER, [](void *ctx, const uint32_t type) {
            platform_raise_irql(Irql::Clock);

            timer::Manager::gShared->tick(kTimebaseInterval * 1000, type);
            platform_kern_tick(type);

            platform_lower_irql(Irql::Passive);
            return true;
        }, apic);

        log("irq handler for apic %p (timer %p) = %08x", apic, timer, token);
    }
}

/**
 * Registers a new irq handler.
 */
uintptr_t Manager::addHandler(const uint32_t irq, bool (*callback)(void *, const uint32_t), void *ctx) {
    REQUIRE(callback, "invalid callback pointer");

    // set up the handler
    Handler handler(irq);
    handler.callback = callback;
    handler.callbackCtx = ctx;

    // yeet it
    RW_LOCK_WRITE_GUARD(this->handlersLock);
    const auto token = this->nextHandlerToken++;
    handler.token = token;

    this->handlers.push_back(handler);

    return token;
}

/**
 * Removes an existing irq handler by its token.
 */
void Manager::removeHandler(const uintptr_t token) {
    RW_LOCK_WRITE_GUARD(this->handlersLock);

    for(size_t i = 0; i < this->handlers.size(); i++) {
        if(this->handlers[i].token == token) {
            this->handlers.remove(i);
            return;
        }
    }

    panic("no irq handler with token %08x", token);
}




/**
 * Handles and routes an ISR appropriately.
 *
 * This function is invoked only for ISRs that require routing; things like spurious interrupts or
 * NMIs have their own handlers. Type codes correspond to values defined in `handlers.h`
 *
 * We will only acknowledge the interrupt (at the controller) ourselves if all callbacks invoked
 * for the IRQ indicate that this is their wish, or if no handlers are found, or we handle the
 * interrupt internally.
 */
void Manager::handleIsr(const uint32_t type) {
    // handle normal interrupts
    RW_LOCK_READ_GUARD(this->handlersLock);

    bool ack = true;
    bool handled = false;

    for(const auto &handler : this->handlers) {
        if(handler.irq == type) {
            handled = true;
            const bool temp = handler.callback(handler.callbackCtx, type);
            if(!temp) ack = temp;
        }
    }

    // ensure something handled the irq
    REQUIRE(handled, "platform irq manager doesn't know how to handle irq %08x", type);

    // acknwledge irq if desired
    if(ack) {
        this->acknowledgeIrq(type);
    }
}

/**
 * Acknowledges an IRQ.
 */
void Manager::acknowledgeIrq(const uint32_t type) {
    // APIC timer or legacy ISA interrupt
    if(type == ISR_APIC_TIMER ||
       (type >= ISR_ISA_0 && type <= ISR_ISA_15)) {
        currentProcessorApic()->endOfInterrupt();
    }
    // IPIs 
    else if(type == ISR_APIC_DISPATCH_IPI) {
        currentProcessorApic()->endOfInterrupt();
    }
    // unhandled isr
    else {
        panic("platform irq manager doesn't know how to ack irq %08x", type);
    }
}

/// Stub to forward into the irq manager
int platform_irq_ack(const uintptr_t token) {
    Manager::gShared->acknowledgeIrq(token);
    return 0;
}
