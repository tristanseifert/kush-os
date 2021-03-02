#ifndef RESOURCE_IRQ_H
#define RESOURCE_IRQ_H

#include <sys/bitflags.hpp>
#include "acpi.h"
#include "log.h"

#include "Resource.h"

namespace acpi::resource {
/**
 * Describes various attributes of an interrupt, including its polarity, triggering mode, and
 * so forth.
 */
enum class IrqMode: uintptr_t {
    Invalid                             = UINTPTR_MAX,
    None                                = 0,

    /// Active high/rising edge polarity
    PolarityHigh                        = (0 << 0),
    /// Active low/falling edge polarity
    PolarityLow                         = (1 << 0),
    /// Both edges
    PolarityBoth                        = (2 << 0),
    /// Mask for interrupt polarity
    PolarityMask                        = (0xF << 0),

    /// Edge triggered
    TriggerEdge                         = (0 << 4),
    /// Level triggered
    TriggerLevel                        = (1 << 4),
    /// Mask for interrupt trigger mode
    TriggerMask                         = (0xF << 4),

    /// The interrupt is capable of waking the system
    WakeCapable                         = (1 << 8),
};
ENUM_FLAGS_EX(IrqMode, uintptr_t);

/**
 * Describes an interrupt resource 
 */
struct Irq: public Resource {
    /// Interrupt trigger mode and polarity
    IrqMode flags = IrqMode::Invalid;
    /// System interrupt number
    uint8_t irq;

    /// Creates an IRQ resource (uninitialized)
    Irq() = default;
    /// Creates an IRQ resource from an ACPI resource table IRQ entry
    Irq(ACPI_RESOURCE_IRQ &in) {
        this->flags = IrqMode::None;
        this->decodeTriggering(in.Triggering);
        this->decodePolarity(in.Polarity);
        if(in.WakeCapable == ACPI_WAKE_CAPABLE) {
            this->flags |= IrqMode::WakeCapable;
        }

        this->irq = in.Interrupts[0];
    }
    /// Creates an IRQ resource from an ACPI resource table extended IRQ entry
    Irq(ACPI_RESOURCE_EXTENDED_IRQ &in) {
        this->flags = IrqMode::None;
        this->decodeTriggering(in.Triggering);
        this->decodePolarity(in.Polarity);
        if(in.WakeCapable == ACPI_WAKE_CAPABLE) {
            this->flags |= IrqMode::WakeCapable;
        }

        this->irq = in.Interrupts[0];
    }

    private:
        /// Decodes the "Triggering" field of an IRQ resource
        void decodeTriggering(const uint8_t in) {
            switch(in) {
                case ACPI_LEVEL_SENSITIVE:
                    this->flags |= IrqMode::TriggerLevel;
                    break;
                case ACPI_EDGE_SENSITIVE:
                    this->flags |= IrqMode::TriggerEdge;
                    break;
                default:
                    Abort("Invalid IRQ trigger mode: %02x", in);
            }
        }

        /// Decodes the interrupt polarity field on an IRQ resource
        void decodePolarity(const uint8_t in) {
            switch(in) {
                case ACPI_ACTIVE_HIGH:
                    this->flags |= IrqMode::PolarityHigh;
                    break;
                case ACPI_ACTIVE_LOW:
                    this->flags |= IrqMode::PolarityLow;
                    break;
                default:
                    Abort("Invalid IRQ polarity: %02x", in);
            }
        }
};
}

#endif
