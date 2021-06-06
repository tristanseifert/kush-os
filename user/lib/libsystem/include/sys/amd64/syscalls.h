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

/// Perform an 8 bit wide port read/write
#define SYS_ARCH_AMD64_PORT_BYTE        (0x01)
/// Perform an 16 bit wide port read/write
#define SYS_ARCH_AMD64_PORT_WORD        (0x02)
/// Perform an 32 bit wide port read/write
#define SYS_ARCH_AMD64_PORT_DWORD       (0x03)
/// Bitmask for the port IO flags to get the port size
#define SYS_ARCH_AMD64_PORT_SIZE_MASK   (0x0F)

#ifdef __cplusplus
extern "C" {
#endif

LIBSYSTEM_EXPORT int Amd64SetThreadLocalBase(const int which, const uintptr_t base);
LIBSYSTEM_EXPORT int Amd64SetThreadLocalBaseFor(const uintptr_t threadHandle, const int which, const uintptr_t base);

LIBSYSTEM_EXPORT int Amd64CopyLoaderInfo(void *outBuf, const size_t outBufLen);

LIBSYSTEM_EXPORT int Amd64UpdateAllowedIoPortsFor(const uintptr_t taskHandle, const void *bitmap, const size_t numBits, const uintptr_t portOffset);
LIBSYSTEM_EXPORT int Amd64UpdateAllowedIoPorts(const void *bitmap, const size_t numBits, const uintptr_t portOffset);

LIBSYSTEM_EXPORT int Amd64PortRead(const uintptr_t port, const uintptr_t flags, uint32_t *read);
LIBSYSTEM_EXPORT int Amd64PortReadB(const uintptr_t port, const uintptr_t flags, uint8_t *read);

LIBSYSTEM_EXPORT int Amd64PortReadW(const uintptr_t port, const uintptr_t flags, uint16_t *read);

LIBSYSTEM_EXPORT int Amd64PortReadL(const uintptr_t port, const uintptr_t flags, uint32_t *read);

LIBSYSTEM_EXPORT int Amd64PortWrite(const uintptr_t port, const uintptr_t flags, const uint32_t write);
LIBSYSTEM_EXPORT int Amd64PortWriteB(const uintptr_t port, const uintptr_t flags, const uint8_t write);
LIBSYSTEM_EXPORT int Amd64PortWriteW(const uintptr_t port, const uintptr_t flags, const uint16_t write);
LIBSYSTEM_EXPORT int Amd64PortWriteL(const uintptr_t port, const uintptr_t flags, const uint32_t write);

#ifdef __cplusplus
}
#endif

#endif
