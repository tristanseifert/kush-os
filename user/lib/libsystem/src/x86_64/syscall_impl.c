#include "sys/syscall.h"
#include <stdint.h>

/**
 * On x86, the syscall calling convention requires that we jump to the syscall stub, mapped by the
 * kernel at 0x00007FFF00000000.
 *
 * The syscall number is passed in %ax; The high 48 bits of %rax are reserved for syscall-specific
 * use. On return, %rax contains the return code.
 *
 * Syscalls may have up to 4 arguments, passed in the %rdi, %rsi, %rdx and %rcx registers,
 * respectively.
 */
#define SYSCALL_STUB_ADDR               0x00007FFF00000000

/**
 * 0 argument syscall; jump directly to the syscall stub.
 */
int __do_syscall0(const uintptr_t number) {
    int ret;
    asm volatile(
        "call  %1\n" :
        // outputs: return code of the syscall
        "=a" (ret) :
        // inputs: stub address, syscall number
        "r" (SYSCALL_STUB_ADDR), "a" (number) :
        // ecx and edx get clobbered
        "ecx", "edx"
    );
    return ret;
}

/**
 * 1 argument syscall; place the first argument into %rdi.
 */
int __do_syscall1(const uintptr_t number, const uintptr_t arg0) {
    int ret;
    asm volatile(
        "call  %1\n" :
        // outputs: return code of the syscall
        "=a" (ret) :
        // inputs: stub address, syscall number, arguments
        "r" (SYSCALL_STUB_ADDR), "a" (number), "Rdi" (arg0) :
        // ecx and edx get clobbered
        "ecx", "edx"
    );
    return ret;
}

/**
 * 2 argument syscall; place the first argument into %rdi, the second to %rsi.
 */
int __do_syscall2(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1) {
    int ret;
    asm volatile(
        "call  %1\n" :
        // outputs: return code of the syscall
        "=a" (ret) :
        // inputs: stub address, syscall number, arguments
        "r" (SYSCALL_STUB_ADDR), "a" (number), "Rdi" (arg0), "Rsi" (arg1) :
        // ecx and edx get clobbered
        "ecx", "edx"
    );
    return ret;
}

/**
 * 3 argument syscall; place arguments into %rdi, %rsi, %rdx.
 */
int __do_syscall3(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2) {
    int ret;
    asm volatile(
        "call  %1\n" :
        // outputs: return code of the syscall
        "=a" (ret) :
        // inputs: stub address, syscall number, arguments
        "r" (SYSCALL_STUB_ADDR), "a" (number), "Rdi" (arg0), "Rsi" (arg1) , "d" (arg2) :
        // no explicitly listed clobbers
    );
    return ret;
}

/**
 * 4 argument syscall; place arguments into %rdi, %rsi, %rdx, %rcx.
 */
int __do_syscall4(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1,
        const uintptr_t arg2, const uintptr_t arg3) {
    int ret;
    asm volatile(
        "call  %1\n" :
        // outputs: return code of the syscall
        "=a" (ret) :
        // inputs: stub address, syscall number, arguments
        "r" (SYSCALL_STUB_ADDR), "a" (number), "Rdi" (arg0), "Rsi" (arg1) , "d" (arg2), "c" (arg3) :
        // no explicitly listed clobbers
    );
    return ret;
}

