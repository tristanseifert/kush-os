/**
 * System calls are done using SYSCALL; it will write to RCX the return address, and to R11 the
 * flags value. You should set R9 to the stack pointer to restore when returning from a system
 * call.
 *
 * The syscall number is passed in %ax; The high 48 bits of %rax are reserved for syscall-specific
 * use. On return, %rax contains the return code.
 *
 * Syscalls may have up to 4 arguments, passed in the %rdi, %rsi, %rdx and %r8 registers,
 * respectively.
 */


.global __do_syscall0
.type __do_syscall0, function
/*
 * intptr_t __do_syscall0(const uintptr_t number)
 *
 * Syscall with no arguments
 */
__do_syscall0:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number to %rax
    mov         %rdi, %rax

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq

.global __do_syscall1
.type __do_syscall1, function
/*
 * intptr_t __do_syscall1(const uintptr_t number, const uintptr_t arg0)
 *
 * Syscall with one argument
 */
__do_syscall1:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number and args
    mov         %rdi, %rax
    mov         %rsi, %rdi

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq

.global __do_syscall2
.type __do_syscall2, function
/*
 * intptr_t __do_syscall2(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1)
 *
 * Syscall with two arguments
 */
__do_syscall2:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number and args
    mov         %rdi, %rax
    mov         %rsi, %rdi
    mov         %rdx, %rsi

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq

.global __do_syscall3
.type __do_syscall3, function
/*
 * intptr_t __do_syscall2(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1,
 *      const uintptr_t arg2)
 *
 * Syscall with three arguments
 */
__do_syscall3:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number and args
    mov         %rdi, %rax
    mov         %rsi, %rdi
    mov         %rdx, %rsi
    mov         %rcx, %rdx

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq

.global __do_syscall4
.type __do_syscall4, function
/*
 * intptr_t __do_syscall2(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1,
 *      const uintptr_t arg2, const uintptr_t arg3)
 *
 * Syscall with four arguments
 */
__do_syscall4:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number and args
    mov         %rdi, %rax
    mov         %rsi, %rdi
    mov         %rdx, %rsi
    mov         %rcx, %rdx
    mov         %r8, %rcx

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq
