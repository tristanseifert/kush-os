/**
 * X86_64 (AMD64) architecture specific calls
 */
#ifndef LIBC_SYSCALLS_ARCH_AMD64_H
#define LIBC_SYSCALLS_ARCH_AMD64_H

#include <_libsystem.h>

#include <stdint.h>
#include <stddef.h>

/// Set the FS thread-local register base
#define SYS_ARCH_AMD64_TLS_FS           (0)
/// Set the GS thread-local register base
#define SYS_ARCH_AMD64_TLS_GS           (1)

#ifdef __cplusplus
extern "C" {
#endif

LIBSYSTEM_EXPORT int Amd64SetThreadLocalBase(const int which, const uintptr_t base);
LIBSYSTEM_EXPORT int Amd64SetThreadLocalBaseFor(const uintptr_t threadHandle, const int which, const uintptr_t base);

#ifdef __cplusplus
}
#endif

#endif
