#ifndef PLATFORM_PC_IRQ_MANAGER_H
#define PLATFORM_PC_IRQ_MANAGER_H

#include "handlers.h"

#include <platform.h>
#include <bitflags.h>
#include <runtime/Vector.h>

#include <arch/rwlock.h>

namespace platform { namespace irq {
class Apic;
class IoApic;


/**
 * Flags for interrupts
 */
ENUM_FLAGS(IrqFlags)
enum class IrqFlags {
    None                                = 0,

    /// Mask for trigger polarity
    PolarityMask                        = (0b1111 << 0),
    /// Polarity: active high
    PolarityHigh                        = (0 << 0),
    /// Polarity: active low
    PolarityLow                         = (1 << 0),

    /// Mark for trigger mode
    TriggerMask                         = (0b1111 << 4),
    /// Trigger mode: edge
    TriggerEdge                         = (0 << 4),
    /// Trigger mode: level
    TriggerLevel                        = (1 << 4),

    /// Mask for the type value
    TypeMask                            = (0xFF << 8),
    /// The interrupt should be mapped as an NMI.
    TypeNMI                             = (1 << 8),
};

/**
 * Handles coordinating interrupt routing between external sources and the final interrupt vectors
 * that are fired.
 *
 * Additionally, this serves as the low-level interrupt trap handler; any received interrupts are
 * converted from platform-specific types to the type the kernel expects.
 */
class Manager {
    friend void ::platform_vm_available();
    friend void ::platform_isr_handle(const uint32_t);
    friend int ::platform_irq_ack(const uintptr_t);

    public:
        /// System timebase, in microseconds
        constexpr static const uint32_t kTimebaseInterval = 2000;

    public:
        Manager();
        ~Manager();

        /// The system contains some legacy PICs or not.
        void setHasLegacyPic(const bool yes);
        /// Initializes a detected local APIC
        void detectedLapic(const uint64_t phys, const uint8_t id, const uint8_t cpu,
                const bool enabled, const bool onlineable);
        /// Initializes a detected IOAPIC
        void detectedIoapic(const uint64_t phys, const uint8_t cpu, const uint32_t irqBase);

        /// Adds a detected interrupt source override.
        void detectedOverride(const uint8_t bus, const uint8_t source, const uint32_t globalIrq, const IrqFlags flags);
        /// Adds a detected NMI configuration.
        void detectedNmi(const uint8_t cpus, const uint8_t lint, const IrqFlags flags);

        /// Registers an interrupt handler for the given interrupt.
        uintptr_t addHandler(const uint32_t irq, bool (*callback)(void *, const uint32_t), void *ctx);
        /// Removes an irq handler.
        void removeHandler(const uintptr_t token);

        /// Gets the global IRQ manager instance.
        static Manager *get() {
            return gShared;
        }

        /// Returns a reference to the APIC of the currently running processor.
        static Apic *currentProcessorApic() {
            // TODO: do this properly
            return gShared->apics[0];
        };

    private:
        /// Info on a single IRQ override
        struct IrqOverride {
            /// Bus (source); 0xFF means no bus
            uint8_t bus = 0xFF;
            /// Bus specific interrupt number (source)
            uint8_t busIrq;

            /// Global irq number (destination)
            uint32_t irqNo;
            /// IRQ trigger level and mode
            IrqFlags flags;
        };

        /// Registered IRQ handler
        struct Handler {
            /// registration token
            uintptr_t token = 0;
            /// IRQ number
            uint32_t irq = 0;

            /// function to invoke; return true to acknowledge the irq
            bool (*callback)(void *, const uint32_t) = nullptr;
            /// context pointer to provide to callback
            void *callbackCtx = nullptr;

            Handler() = default;
            Handler(uint32_t _irq) : irq(_irq) {}
        };

    private:
        // Configures all APIC registers
        void configure();
        // Installs interrupt routing overrides
        void configureOverrides();
        // Installs the x86 vector interrupt handlers
        void installHandlers();
        // enables the APICs
        void enable();
        // Configures any APIC-local timers to operate as the system timebase.
        void setupTimebase();

        /// Handles an ISR.
        void handleIsr(const uint32_t type);
        /// Acknowledges an IRQ
        void acknowledgeIrq(const uint32_t type);

    private:
        static void init();
        static void setupIrqs();

        static Manager *gShared;

    private:
        /// lock on irq handlers
        DECLARE_RWLOCK(handlersLock);
        /// list of all installed handlers (TODO: this could be stored more efficiently)
        rt::Vector<Handler> handlers;
        /// token for the next irq handler
        uintptr_t nextHandlerToken = 1;


        /// all IOAPICs in the system (usually, only one)
        rt::Vector<IoApic *> ioapics;
        /// all APICs in the system (one per CPU core)
        rt::Vector<Apic *> apics;

        /// IRQ overrides detected from ACPI tables
        rt::Vector<IrqOverride> overrides;

        /// physical base address for local APICs
        uint64_t apicPhysBase = 0;
        /// base address for local APICs
        void *apicVirtBase = nullptr;

        /// when set, we have legacy PICs that we need to disable
        bool has8259 = false;
};
}}

#endif
