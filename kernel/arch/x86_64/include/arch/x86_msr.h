#ifndef ARCH_X86_MSR_H
#define ARCH_X86_MSR_H

#include <stdint.h>

/// extended feature enable register
#define X86_MSR_EFER                    0xC0000080

/// EFER flag for SYSCALL/SYSRET
#define X86_MSR_EFER_SCE                (1 << 0)
/// EFER flag for NX bit
#define X86_MSR_EFER_NX                 (1 << 11)

/// Base address of the %fs segment
#define X86_MSR_FSBASE                  0xC0000100
/// Base address of the %gs segment
#define X86_MSR_GSBASE                  0xC0000101
/// Base address of the kernel space %gs segment (for use with swapgs)
#define X86_MSR_KERNEL_GSBASE           0xC0000102

/// Ring 0 and 3 segment bases
#define X86_MSR_IA32_STAR               0xC0000081
/// %RIP value to load for the 64-bit SYSCALL entry
#define X86_MSR_IA32_LSTAR              0xC0000082
/// %RIP value for SYSCALL entry from compatibility mode code
#define X86_MSR_IA32_CSTAR              0xC0000083
/// Low 32 bits define which bits to mask off in RFLAGS on SYSCALL
#define X86_MSR_IA32_FMASK              0xC0000084

/**
 * Writes a model-specific register.
 */
static inline void x86_msr_write(const uint32_t msr, const uint32_t lo, const uint32_t hi) {
    asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

/**
 * Reads a model-specific register.
 */
static inline void x86_msr_read(const uint32_t msr, uint32_t *lo, uint32_t *hi) {
    asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

/**
 * Reads the system timestamp counter.
 */
static inline uint64_t x86_rdtsc() {
    uint32_t lo, hi;
    // RDTSC copies contents of 64-bit TSC into EDX:EAX
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));

    return (lo & 0xFFFFFFFF) | (((uint64_t) hi) << 0x20);
}

#endif
