#ifndef ARCH_X86_MSR_H
#define ARCH_X86_MSR_H

#include <stdint.h>

/// extended feature enable register
#define X86_MSR_EFER                    0xC0000080

/// EFER flag for NX bit
#define X86_MSR_EFER_NX                 (1 << 11)

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

/**
 * Reads a CPUID page.
 */
static inline void x86_cpuid(const uint32_t page, uint32_t *a, uint32_t *d) {
    asm volatile("cpuid":"=a"(*a),"=d"(*d):"a"(page):"ecx","ebx");
}

#endif
