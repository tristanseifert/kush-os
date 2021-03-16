#ifndef PLATFORM_PC64_IRQ_IOAPIC_H
#define PLATFORM_PC64_IRQ_IOAPIC_H

#include <stddef.h>
#include <stdint.h>

#include "Manager.h"
#include "../acpi/Parser.h"

namespace vm {
class MapEntry;
}

namespace platform {
class IoApic {
    public:
        /// Vector number for the first IOAPIC interrupt
        constexpr static const uint8_t kFirstVector = 0x40;

    public:
        IoApic(const uintptr_t base, const uint32_t irqBase, const uint8_t id);
        ~IoApic();

        /// Checks whether this IOAPIC maps the given interrupt
        bool handlesIrq(const uint8_t irq) {
            return (this->irqBase < irq) && ((irq - this->irqBase) < this->numIrqs);
        }

        /// Remaps an irq
        void remap(const uint8_t irq, const uint32_t dest, const IrqFlags flags);
        /// Configures whether an irq is masked
        void setIrqMasked(const uint8_t irq, const bool masked);

    private:
        /// Format of the 64-bit redirection entries
        union RedirectionEntry {
            struct {
                uint64_t vector       : 8; //
                uint64_t delvMode     : 3; //
                uint64_t destMode     : 1; //
                uint64_t delvStatus   : 1; //
                uint64_t pinPolarity  : 1; //
                uint64_t remoteIRR    : 1; //
                uint64_t triggerMode  : 1; //
                uint64_t mask         : 1;
                uint64_t reserved     : 39;
                uint64_t destination  : 8; //
            };

            // access as two 32-bit words (to write to regs)
            struct {
                uint32_t lower;
                uint32_t upper;
            };
        } __attribute__((packed));

    private:
        /// Reads the IOAPIC register at the given offset
        inline uint32_t read(const size_t regOff) {
            *((uint32_t volatile*) this->base) = regOff;
            return *(uint32_t volatile *) (reinterpret_cast<uintptr_t>(this->base) + 0x10);
        }

        /// Writes the IOAPIC register at the given offset
        inline void write(const size_t regOff, const uint32_t data) {
            *((uint32_t volatile*) this->base) = regOff;
            *((uint32_t volatile *) (reinterpret_cast<uintptr_t>(this->base) + 0x10)) = data;
        }

        /// Reads a redirection entry from the APIC.
        const RedirectionEntry getRedirEntry(const size_t offset);
        /// Writes a redirection entry.
        void setRedirEntry(const size_t offset, const RedirectionEntry &entry);

        void mapIsaIrqs();
        void installOverrides();
        void addOverride(const AcpiParser::MADT::IrqSourceOverride *);

    private:
        /// is initialization logged?
        static bool gLogInit;
        /// are setting of redirection entries logged?
        static bool gLogSet;

    private:
        /// physical base address
        uintptr_t physBase;
        /// system irq vector base
        uintptr_t irqBase;
        /// IOAPIC ID
        uintptr_t id;

        /// Number of interrupts handled by this IOAPIC
        size_t numIrqs = 0;

        /// Mapping object for the APIC's registers
        vm::MapEntry *vm;
        /// APIC register base
        void *base;
};
}

#endif
