#ifndef ARCH_X86_SYSCALL_SYSCALLS_H
#define ARCH_X86_SYSCALL_SYSCALLS_H

#include <stdint.h>

#include <sys/Syscall.h>

namespace arch::syscall {
intptr_t UpdateThreadTlsBase(const uintptr_t threadHandle, const bool gs, const uintptr_t base);
}

#endif
