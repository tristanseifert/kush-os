#ifndef PLATFORM_PC64_IRQ_LOCALAPIC_H
#define PLATFORM_PC64_IRQ_LOCALAPIC_H

#include <platform.h>

#include <stddef.h>
#include <stdint.h>

#include <log.h>

namespace vm {
class MapEntry;
}

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
            if(gLogRegIo) { [[unlikely]]
                log("LAPIC %2u %s: %04x -> %08x", this->id, "write", reg, value);
            }
            this->base[reg / 4] = value;
        }
        /// Reads the given APIC register.
        inline const uint32_t read(const size_t reg) {
            const auto val = this->base[reg / 4];
            if(gLogRegIo) { [[unlikely]]
                log("LAPIC %2u %s: $%4x -> %08x", this->id, "read", reg, val);
            }
            return val;
        }

        /// Configures the core's NMI lines
        void configNmi(const uintptr_t cpuId);
        /// Enable the APIC
        void enable();

    private:
        // XXX: this should go somewhere better than being duplicated everywhere...
        /// Base address of the physical memory identity mapping zone
        constexpr static uintptr_t kPhysIdentityMap = 0xffff800000000000;

        /// whether initialization logs are enabled
        static bool gLogInit;
        /// whether reads/writes to LAPIC registers are logged
        static bool gLogRegIo;

    private:
        /// ID of this local APIC
        uintptr_t id;
        /// Base of local APIC register space
        volatile uint32_t *base;

        /// VM mapping of the phys base
        vm::MapEntry *vmEnt = nullptr;
};
}

#endif
