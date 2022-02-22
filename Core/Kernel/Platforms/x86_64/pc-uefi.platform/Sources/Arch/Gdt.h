#ifndef KERNEL_PLATFORM_UEFI_ARCH_GDT_H
#define KERNEL_PLATFORM_UEFI_ARCH_GDT_H

/// Supervisor code segment
#define GDT_KERN_CODE_SEG       0x08
/// Supervisor data segment
#define GDT_KERN_DATA_SEG       0x10
/// User code segment
#define GDT_USER_CODE_SEG       0x18
/// User data segment
#define GDT_USER_DATA_SEG       0x20
/// User 64-bit code segment
#define GDT_USER_CODE64_SEG     0x28
/// First TSS segment
#define GDT_FIRST_TSS           0x30

#ifndef ASM_FILE
#include <stddef.h>
#include <stdint.h>

namespace Platform::Amd64Uefi {
struct GdtDescriptor;
struct Tss;

/**
 * Handles a processor's Global Descriptor Table (GDT)
 *
 * Since we live in amd64 long mode, the GDT is relatively bare-bones. We allocate all the
 * usual segments, as well as a task state segment (TSS) so that separate interrupt stacks can be
 * specified.
 */
class Gdt {
    public:
        static void Init();

        static void InstallTss(const size_t i, Tss *tss);
        static void InitTss(Tss *tss);

        static void Set32(const size_t idx, const uint32_t base, const uint32_t limit,
                const uint8_t flags, const uint8_t gran);
        static void Set64(const size_t idx, const uintptr_t base, const uint32_t limit,
                const uint8_t flags, const uint8_t granularity);
        static void Load(const size_t numTss = 1);
        static void ActivateTask(const size_t task);

    private:
        /**
         * Flushes the cached GDT registers in the CPU by reloading them.
         *
         * This is implemented in `GdtHelpers.S`.
         */
        static void Flush();

    public:
        /**
         * Number of words (8 byte units) to allocate for an interrupt stack
         *
         * Interrupt stacks are pointed to by the processor's TSS, for the express purpose of
         * providing a known-good stack for handling of exceptions and interrupts.
         */
        constexpr static const size_t kIrqStackSize{512};

        /**
         * Total number of GDT entries to allocate
         *
         * Note that the first 6 entries are reserved for segment descriptors; the remainder are
         * reserved for TSS's.
         */
        constexpr static const size_t kGdtSize{64};

        /**
         * Total number of TSS slots to allocate
         *
         * Each TSS is used to provide interrupt stacks, and should be allocated for each processor
         * individually.
         */
        constexpr static const size_t kTssSlots{kGdtSize - (GDT_FIRST_TSS / 8)};

    private:
        /// Are writes to the GDT logged?
        constexpr static const bool kLogSet{false};
        /// Are GDT loads logged?
        constexpr static const bool kLogLoad{false};

        /// Storage for the system GDT
        static GdtDescriptor gGdt[kGdtSize];
        /// Next TSS index to allocate
        static size_t gNextTss;
};
}

#endif
#endif
