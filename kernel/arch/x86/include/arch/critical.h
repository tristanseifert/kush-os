#ifndef ARCH_X86_CRITICAL_H
#define ARCH_X86_CRITICAL_H

#include "private.h"

/**
 * Declares a critical section
 */
struct CriticalSection {
    uint8_t dummy;
};

#define CRITICAL_ENTER() asm volatile("cli" ::: "memory");
#define CRITICAL_EXIT() asm volatile("sti" ::: "memory");

#endif
