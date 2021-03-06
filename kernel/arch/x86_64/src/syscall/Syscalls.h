#ifndef ARCH_X86_SYSCALL_SYSCALLS_H
#define ARCH_X86_SYSCALL_SYSCALLS_H

#include <stdint.h>

#include <sys/Syscall.h>

namespace arch::syscall {
int UpdateTaskIopb(const sys::Syscall::Args *, const uintptr_t);
int UpdateThreadTlsBase(const sys::Syscall::Args *, const uintptr_t);
}

#endif
