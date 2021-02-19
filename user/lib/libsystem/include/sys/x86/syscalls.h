/**
 * X86 architecture specific calls
 */
#ifndef LIBC_SYSCALLS_ARCH_X86_H
#define LIBC_SYSCALLS_ARCH_X86_H

#include <_libc.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

LIBC_EXPORT int X86UpdateIopb(const void *bitmap, const uintptr_t bits, const uintptr_t offset);
LIBC_EXPORT int X86UpdateIopbFor(const uintptr_t taskHandle, const void *bitmap,
        const uintptr_t bits, const uintptr_t offset);


#ifdef __cplusplus
}
#endif

#endif
