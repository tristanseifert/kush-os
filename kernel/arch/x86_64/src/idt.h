#ifndef ARCH_AMD64_IDT_H
#define ARCH_AMD64_IDT_H

#include <stddef.h>
#include <stdint.h>

/// IDT flags suitable for an ISR: present, DPL=0, 64-bit interrupt gate
#define IDT_FLAGS_ISR                   0x8E
/// IDT flags suitable for an exception/trap: present, DPL=0, 64-bit trap gate
#define IDT_FLAGS_TRAP                  0x8F

namespace arch {
/// 64-bit long mode interrupt descriptor
typedef struct arch_idt_descriptor_64 {
    /// ofset bits 0..15
    uint16_t offset1;
    /// a code segment selector in GDT/LDT
    uint16_t selector;
    /// which interrupt stack table to use, if any
    uint8_t ist;
    /// type and attributes
    uint8_t flags;
    /// offset bits 16..32
    uint16_t offset2;
    /// offset bits 33..63
    uint32_t offset3;

    // reserved bits: keep as zero
    uint32_t reserved;
} __attribute__((packed)) idt_entry64_t;


/**
 * Interface to the 64-bit interrupt descriptor table. We allocate storage for all 256 possible
 * vectors, although we likely won't use them all.
 *
 * TODO: ensure this is placed in per-processor data
 */
class Idt {
    /// Number of IDTs to allocate
    constexpr static const size_t kNumIdt = 256;

    public:
        /// Stack to use for an interrupt
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

    public:
        /// Initialize the BSP IDT and activates it.
        static void Init();
        /// Initializes a new IDT, sets it up with the exception handler entries, and loads it.
        Idt();

        /// Sets an IDT entry
        void set(const size_t entry, const uintptr_t addr, const uintptr_t seg, const uintptr_t flags, const Stack stack = Stack::None);

        /// Activates the IDT.
        void load();

    private:
        static idt_entry64_t gIdts[kNumIdt];

        /// whether loading of the IDT is logged
        static bool gLogLoad;
        /// whether inserting an entry in the IDT is logged
        static bool gLogSet;

    private:
        /// IDT storage
        idt_entry64_t storage[kNumIdt] __attribute__((aligned((64))));
};

extern Idt *gBspIdt;
}

#endif
