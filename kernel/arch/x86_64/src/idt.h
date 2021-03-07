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
        /// Initialize the IDT memory and load it into the processor register.
        static void Init();
        /// Sets an IDT entry
        static void Set(const size_t entry, const uintptr_t addr, const uintptr_t seg, const uintptr_t flags, const uintptr_t stack = 0);

    private:
        static idt_entry64_t gIdts[kNumIdt] __attribute__((aligned(64)));
};
}

#endif
