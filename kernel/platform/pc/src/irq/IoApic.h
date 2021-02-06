#ifndef PLATFORM_PC_IRQ_IOAPIC_H
#define PLATFORM_PC_IRQ_IOAPIC_H

#include <stddef.h>
#include <stdint.h>

#include "Manager.h"

namespace platform { namespace irq {
/**
 * Handles interacting with the system's IOAPIC.
 */
class IoApic {
    public:
        /// first vector number of IOAPIC interrupts
        constexpr static const uint8_t kFirstVector = 0x30;

    public:
        IoApic(const uint64_t physBase, const uint32_t irqBase);

        /// Checks whether this IOAPIC maps the given interrupt
        bool handlesIrq(const uint8_t irq) {
            return (this->irqBase < irq) && ((irq - this->irqBase) < this->numIrqs);
        }

        /// Remaps an irq
        void remap(const uint8_t irq, const uint32_t dest, const IrqFlags flags);

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
        uint32_t read(const size_t regOff) {
            *((uint32_t volatile*) this->baseAddr) = regOff;
            return *(uint32_t volatile *) (this->baseAddr + 0x10);
        }

        /// Writes the IOAPIC register at the given offset
        void write(const size_t regOff, const uint32_t data) {
            *((uint32_t volatile*) this->baseAddr) = regOff;
            *((uint32_t volatile *) (this->baseAddr + 0x10)) = data;
        }

        void mapIsaIrqs();

    private:
        static size_t numAllocated;

    private:
        // physical base address of the IOAPIC
        uint64_t physBase;
        // VM base address to IOAPIC regs
        uint32_t baseAddr = 0;

        // APIC ID
        uint8_t id = 0;
        // interrupt base, i.e. the first interrupt number handled by the APIC
        uint32_t irqBase;
        // total number of interrupts handled by this APIC
        uint16_t numIrqs;
};
}}

#endif
