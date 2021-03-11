#ifndef PLATFORM_PC64_IRQ_MANAGER_H
#define PLATFORM_PC64_IRQ_MANAGER_H

#include <bitflags.h>

namespace platform {
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

    public:
        /// Initialize the global IRQ manager.
        static void Init();

        /// Return the shared (among all processors) IRQ manager
        static inline IrqManager *the() {
            return gShared;
        }

    private:
        /// global IRQ manager instance
        static IrqManager *gShared;
};
};

#endif
