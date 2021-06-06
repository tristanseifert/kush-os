#ifndef PLATFORM_PC64_IRQ_MANAGER_H
#define PLATFORM_PC64_IRQ_MANAGER_H

#include <bitflags.h>
#include <stdint.h>

#include <runtime/Vector.h>

#include "../acpi/Parser.h"

namespace platform {
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
 * IRQ manager handles routing of interrupts (including management of system and core-local
 * interrupt controller hardware) to user-specified callbacks.
 */
class IrqManager {
    friend class AcpiParser;
    friend class CoreLocalIrqRegistry;

    public:
        /// Initialize the global IRQ manager.
        static void Init();
        /// Initializes all IOAPICs listed in the ACPI tables.
        static void InitSystemControllers();
        /// Detects the CPU local APIC and initializes it
        static void InitCoreLocalController();

        /// Return the shared (among all processors) IRQ manager
        static inline IrqManager *the() {
            return gShared;
        }

    private:
        void initIoApic(const AcpiParser::MADT::IoApic *record);
        void initLapic(const uintptr_t lapicPhys, const uintptr_t cpuId,
                const AcpiParser::MADT::LocalApic *record);

        // Masks or unmasks a system interrupt
        void setMasked(const uintptr_t vector, const bool isMasked);

    private:
        /// global IRQ manager instance
        static IrqManager *gShared;

    private:
        /// IOAPICs in the system
        rt::Vector<IoApic *> ioapics;
};
};

#endif
