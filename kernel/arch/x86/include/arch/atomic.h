#ifndef ARCH_X86_ATOMIC_H
#define ARCH_X86_ATOMIC_H

#include <stdint.h>

// RW barrier
#define barrier() asm volatile("": : :"memory")

#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
//#define atomic_xadd(P, V) __atomic_fetch_add((P), (V), __ATOMIC_ACQ_REL)
#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1) 
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))


/// Atomic 32-bit exchange
static inline __attribute__((always_inline)) uint32_t xchg_32(volatile void *ptr, uint32_t x) {
    asm volatile("xchgl %0, %1" :"=r" ((uint32_t) x) :"m" (*(volatile uint32_t *) ptr), "0" (x)
                 :"memory");
    return x;
}

/// Atomic 16-bit exchange
static inline __attribute__((always_inline)) uint16_t xchg_16(void *ptr, uint16_t x) {
    asm volatile("xchgw %0,%1" : "=r" ((uint16_t) x) :"m" (*(volatile uint16_t *) ptr),
                 "0" (x) : "memory");

    return x;
}

#endif
