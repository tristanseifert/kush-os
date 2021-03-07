#ifndef ARCH_X86_GDT_H
#define ARCH_X86_GDT_H

#define GDT_KERN_CODE_SEG       0x08
#define GDT_KERN_DATA_SEG       0x10
#define GDT_USER_CODE_SEG       0x18
#define GDT_USER_DATA_SEG       0x20
#define GDT_RESERVED            0x28

#define GDT_FIRST_TSS           0x30

#ifndef ASM_FILE
#include <stdint.h>
#include <stddef.h>

/**
 * 32-bit i386 GDT entry
 */
typedef struct arch_gdt_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_descriptor_t;

/**
 * 64-bit extended GDT entry; these are used when the system bit (bit 4 of the Access flags) is
 * clear, forming a 16-bit descriptor. This contains a full 64-bit pointer, and can be used for
 * TSS segments. (Code/data segments in long mode are ignored.)
 */
typedef struct arch_gdt_descriptor_64 {
    /// limit 15..0
    uint16_t limit0;
    /// base 15..0
    uint16_t base0;
    /// base 23..16
    uint8_t base1;
    /// present flag, DPL, type
    uint8_t typeFlags;
    /// granularity, available flag, bits 19..16 of limit
    uint8_t granularityLimit;
    /// base address 31..24
    uint8_t base2;
    /// base address 63..32
    uint32_t base3;
    /// reserved (always zero)
    uint32_t reserved;
} __attribute__((packed)) gdt_descriptor64_t;


/**
 * Task state structure for amd64 mode. The only part of this structure that we really care about
 * and use are the interrupt stacks.
 *
 * All reserved fields should be initialized to zero.
*/
typedef struct amd64_tss {
    uint32_t reserved1;

    // stack pointers
    uint32_t rsp0Low, rsp0High;
    uint32_t rsp1Low, rsp1High;
    uint32_t rsp2Low, rsp2High;

    uint32_t reserved2[2];

    // interrupt stacks
    struct {
        uint32_t low, high;
    } __attribute__((packed)) ist[7];

    uint32_t reserved3[2];

    // IO map offset (should be sizeof(amd64_tss) as we don't use it). use high word
    uint32_t ioMap;
} __attribute__((packed)) amd64_tss_t;

namespace arch {
/**
 * In 64-bit mode, the GDT is basically unused, but we still have pointers to TSS structures for
 * each processor, so they can have known-good interrupt and exception stacks.
 */
class Gdt {
    public:
        /// Initializes the global GDT and loads it.
        static void Init();

        /// Initializes a TSS.
        static void InitTss(amd64_tss_t *tss);
        /// Installs the given TSS at the specified index.
        static void InstallTss(const size_t i, amd64_tss_t *tss);

    private:
        /// Sets a standard 32-bit segment GDT entry.
        static void Set32(const size_t idx, const uint32_t base, const uint32_t limit, const uint8_t flags, const uint8_t gran);

        /// Sets a 64-bit GDT entry.
        static void Set64(const size_t idx, const uintptr_t base, const uint32_t limit, const uint8_t flags, const uint8_t granularity);

        /// Loads the GDT into the processor
        static void Load(const size_t numTss = 1);
        /// Switches to the TSS with the given index.
        static void ActivateTask(const size_t task);

    private:
        /// Total number of GDT entries to allocate
        constexpr static const size_t kGdtSize = 128;
        /// Storage for the system's GDT
        static gdt_descriptor_t gGdt[kGdtSize];

        /// whehter GDT loads are logged
        static bool gLogLoad;
};
}

#endif // ASM_FILE
#endif
