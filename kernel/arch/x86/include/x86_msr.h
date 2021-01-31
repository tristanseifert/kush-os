#ifndef ARCH_X86_MSR_H
#define ARCH_X86_MSR_H

#include <stdint.h>

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
