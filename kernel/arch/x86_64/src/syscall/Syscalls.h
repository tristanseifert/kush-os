#ifndef ARCH_X86_SYSCALL_SYSCALLS_H
#define ARCH_X86_SYSCALL_SYSCALLS_H

#include <stdint.h>

#include <sys/Syscall.h>
#include <handle/Manager.h>

namespace arch::syscall {
intptr_t UpdateThreadTlsBase(const uintptr_t threadHandle, const bool gs, const uintptr_t base);
intptr_t GetLoaderInfo(void *outBuf, const size_t outBufLen);

intptr_t UpdateIoPermission(const Handle taskHandle, const uint8_t *bitmap, const size_t numBits,
        const size_t portOffset, const uintptr_t flags);
intptr_t LockIoPermission(const Handle taskHandle);
intptr_t IoPortRead(const uintptr_t port, const uintptr_t flags, uint32_t *outValue);
intptr_t IoPortWrite(const uintptr_t port, const uintptr_t flags, const uint32_t value);
}

#endif
