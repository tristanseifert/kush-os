/**
 * System calls are done using SYSCALL; it will write to RCX the return address, and to R11 the
 * flags value. You should set R9 to the stack pointer to restore when returning from a system
 * call.
 *
 * The syscall number is passed in %rax; on return, %rax holds the status code.
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
 * intptr_t __do_syscall1(const uintptr_t arg0, const uintptr_t number)
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
 * intptr_t __do_syscall2(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t number)
 *
 * Syscall with two arguments
 */
__do_syscall2:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number and args
    mov         %rdx, %rax

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq

.global __do_syscall3
.type __do_syscall3, function
/*
 * intptr_t __do_syscall3(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2,
    const uintptr_t number)
 *
 * Syscall with three arguments
 */
__do_syscall3:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number
    mov         %rcx, %rax

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq

.global __do_syscall4
.type __do_syscall4, function
/*
 * intptr_t __do_syscall4(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2,
 *      const uintptr_t arg3, const uintptr_t number)
 *
 * Syscall with four arguments
 */
__do_syscall4:
    // back up stack ptr
    mov         %rsp, %r9

    // move syscall number and 4th argument
    mov         %r8, %rax
    mov         %rcx, %r10

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq


.global __do_syscall5
.type __do_syscall5, function
/*
 * intptr_t __do_syscall5(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2,
 *      const uintptr_t arg3, const uintptr_t arg4, const uintptr_t number)
 *
 * Syscall with five arguments
 */
__do_syscall5:
    // move syscall number and fourth argument
    mov         %r9, %rax
    mov         %rcx, %r10

    // back up stack ptr
    mov         %rsp, %r12

    // perform call
    syscall

    // restore stack
    mov         %r9, %rsp
    retq
