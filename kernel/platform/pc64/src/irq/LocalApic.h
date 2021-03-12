#ifndef PLATFORM_PC64_IRQ_LOCALAPIC_H
#define PLATFORM_PC64_IRQ_LOCALAPIC_H

#include <platform.h>

#include <stddef.h>
#include <stdint.h>

#include <log.h>

namespace platform {
/**
 * Handles a processor-local interrupt controller.
 *
 * These contain the logic for handling NMIs, inter-processor interrupts, as well as providing the
 * necessary interfaces for the core-local timers.
 */
class LocalApic {
    public:
        /// NMI interrupt vector
        constexpr static const uint8_t kVectorNMI = 0xDF;
        /// Spurious vector number
        constexpr static const uint8_t kVectorSpurious = 0xFF;

    public:
        LocalApic(const uintptr_t lapicId, const uintptr_t cpuId, const uintptr_t phys);
        ~LocalApic();

        void updateTpr(const Irql irql);

        /// Return the current core's LAPIC
        static LocalApic *the();

    private:
        /// Writes the given APIC register.
        inline void write(const size_t reg, const uint32_t value) {
            //log("write %04x -> %08x (%p %p %p)", reg, value, this, this->base, &this->base);
            this->base[reg / 4] = value;
        }
        /// Reads the given APIC register.
        inline const uint32_t read(const size_t reg) {
            return this->base[reg / 4];
        }

        /// Configures the core's NMI lines
        void configNmi(const uintptr_t cpuId);
        /// Enable the APIC
        void enable();

    private:
        // XXX: this should go somewhere better than being duplicated everywhere...
        /// Base address of the physical memory identity mapping zone
        constexpr static uintptr_t kPhysIdentityMap = 0xffff800000000000;

    private:
        /// ID of this local APIC
        uintptr_t id;
        /// Base of local APIC register space
        volatile uint32_t *base;
};
}

#endif
