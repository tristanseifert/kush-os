#ifndef KERNEL_PLATFORM_UEFI_ARCH_IDT_H
#define KERNEL_PLATFORM_UEFI_ARCH_IDT_H

#include <stddef.h>
#include <stdint.h>

#include "IdtTypes.h"

namespace Platform::Amd64Uefi {
/**
 * Represent the processor's Interrupt Descriptor Table (IDT) for mapping interrupts
 *
 * Each processor should receive its own IDT, with its own interrupt handlers. However, the first
 * 32 entries are common to all processors, for processor exceptions. These dispatch into the
 * kernel's generic exception handler.
 */
class Idt {
    public:
        /// Definitions of which interrupt stacks to use for an interrupt routine
        enum class Stack: uint8_t {
            /// Do not use an interrupt stack
            None                        = 0,
            /// First interrupt stack: exceptions
            Stack1                      = 1,
            /// Second interrupt stack: faults
            Stack2                      = 2,
            /// Third interrupt stack: NMI
            Stack3                      = 3,
            /// Fourth interrupt stack: MCE/Debug
            Stack4                      = 4,
            /// Fifth interrupt stack: IPIs
            Stack5                      = 5,
            /// Sixth interrupt stack: General IRQs
            Stack6                      = 6,
            Stack7                      = 7,
        };

        /**
         * IDT flags for an ISR
         *
         * Present, DPL=0, 64-bit interrupt gate
         */
        constexpr static const uint8_t kIsrFlags{0x8E};

        /**
         * IDT flags for an exception/trap handler
         *
         * Present, DPL=0, 64-bit trap gate
         */
        constexpr static const uint8_t kTrapFlags{0x8F};

        /**
         * Total number of IDT entries to reserve space for
         */
        constexpr static const size_t kNumIdt{256};

    public:
        /// Initialize the BSP IDT and activates it.
        static void InitBsp();
        /// Initializes a new IDT, sets it up with the exception handler entries, and loads it.
        Idt();

        /// Sets an IDT entry
        void set(const size_t entry, const uintptr_t addr, const uintptr_t seg,
                const uintptr_t flags, const Stack stack = Stack::None);

        /// Activates the IDT.
        void load();

    private:
        /// IDT entries
        IdtEntry storage[kNumIdt] __attribute__((aligned(64)));

        /// Whether all writers to the IDT are logged
        constexpr static const bool kLogSet{true};
        /// Whether the IDT loading is logged
        constexpr static const bool kLogLoad{true};
};
}

#endif
