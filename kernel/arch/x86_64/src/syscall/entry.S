#define ASM_FILE
#include "Handler.h"

/// Macro to save state for syscall
.macro state_save
    push        %rbp
    push        %rbx
    push        %r12
    push        %r13
    push        %r14
    push        %r15
.endm
/// Macro to restore syscall state, companion to state_save
.macro state_restore
    pop         %r15
    pop         %r14
    pop         %r13
    pop         %r12
    pop         %rbx
    pop         %rbp
.endm

/**
 * The syscall number is passed in %ax; the high 48 bits of %rax are available for the syscall to
 * use as flags. On return, %rax contains the status code.
 *
 * Syscalls may have up to 4 arguments, passed in the %rdi, %rsi, %rdx and %rcx registers,
 * respectively.
 */
.global arch_syscall_entry
.align 0x40 // align to cache line
arch_syscall_entry:
    // swap thread-local stuff
    cli
    swapgs

    // yeet
    sti

    // save registers
    state_save
    xor         %rbp, %rbp

    // stack the args (in reverse order)
    push        %rcx
    push        %rdx
    push        %rsi
    push        %rdi

    // push syscall info and invoke the handler
    push        %rsp
    push        %rax

    call        arch_syscall_handle

    // clear stack frame
    add         $16+32, %esp

    // restore state and exit
    state_restore
    jmp         arch_syscall_exit

/**
 * Common return handler for syscalls.
 *
 * We don't actually really have to do anything here; once we jump here, the register state has
 * been restored to exactly how it was when we entered the syscall.
 *
 * The instruction will fault if we try to return to a page marked as supervisor-only access, or
 * use a stack that's not mapped for that process.
 */
arch_syscall_exit:
    // restore values
    mov         %ebp, %ecx
    mov         $0xBF5F0005, %edx

    // restore gs
    cli
    swapgs

    // return from syscall
    sti
    sysexit

