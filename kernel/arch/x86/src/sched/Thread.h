#ifndef ARCH_X86_THREAD_H
#define ARCH_X86_THREAD_H

#include "ThreadState.h"

extern "C" void x86_thread_end() __attribute__((noreturn));

extern "C" void x86_switchto(arch::ThreadState *to) __attribute__((noreturn));
extern "C" void x86_switchto_save(arch::ThreadState *from, arch::ThreadState *to);

extern "C" void x86_ring3_return(const uintptr_t pc, const uintptr_t stack, const uintptr_t arg) __attribute__((noreturn));

extern "C" void x86_dpc_stub() __attribute__((noreturn));

#endif
