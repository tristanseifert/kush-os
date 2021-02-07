#ifndef PLATFORM_PC_IRQ_APIC_H
#define PLATFORM_PC_IRQ_APIC_H

/// APIC base address MSR number
#define IA32_APIC_BASE_MSR 0x1B
/// Processor is BSP flag
#define IA32_APIC_BASE_MSR_BSP 0x100 
/// APIC enable flag
#define IA32_APIC_BASE_MSR_ENABLE 0x800

#include <stdint.h>

#include "Manager.h"

namespace platform {
namespace timer {
class LocalApicTimer;
}

namespace irq {
/**
 * Represents a processor-local APIC (LAPIC.) Since these are local to the processor, they can
 * only be modified on that processor core.
 *
 * Nevertheless, we create an instance of this class for every detected APIC to create the
 * mapping between CPU IDs and APICs, and some other metadata.
 */
class Apic {
    friend class timer::LocalApicTimer;

    public:
        /// Spurious interrupt vector number
        constexpr static const uint8_t kVectorSpurious = 0xFF;
        /// NMI interrupt vector
        constexpr static const uint8_t kVectorNMI = 0xDF;

    public:
        Apic(void *virtBase, const uint8_t apicCpuId, const uint8_t apicId, const bool enabled,
                const bool onlineable);
        ~Apic();

        /// Enables interrupt handling via the APIC
        void enable();
        /// Signals an end-of-interrupt to the APIC
        void endOfInterrupt();

        /// Maps an NMI into one of the LINT# as an NMI
        void mapNmi(const uint8_t lint, const IrqFlags flags);

        /// Returns the APIC ID
        const uint8_t getId() const {
            return this->id;
        }
        /// Returns the processor ID for this APIC
        const uint8_t getProcessor() const {
            return this->processor;
        }

        /// Gets the APIC timer, if there is one
        auto getTimer() const {
            return this->timer;
        }

    private:
        /// Writes the given APIC register.
        inline void write(const uint32_t reg, const uint32_t value) {
            this->base[reg / 4] = value;
        }
        /// Reads the given APIC register.
        inline const uint32_t read(const uint32_t reg) {
            return this->base[reg / 4];
        }

        /// Measures the frequency of the core local timer
        void measureTimerFreq();

    private:
        /// APIC ID
        uint8_t id;
        /// this APIC's processor ID
        uint8_t processor;
        /// set if the processor is the BSP (i.e. the one booting the system)
        bool isBsp = false;

        /// VM base address to APIC regs
        uint32_t *base = nullptr;

        /// timer component of the APIC
        timer::LocalApicTimer *timer = nullptr;
};
}}

#endif
