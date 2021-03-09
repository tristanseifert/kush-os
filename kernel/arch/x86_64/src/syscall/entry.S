#define ASM_FILE
#include "Handler.h"
#include "PerCpuInfo.h"

/// Macro to save state for syscall
.macro state_save
    pushq        %rbx
    pushq        %rcx
    pushq        %rbp
    pushq        %r9
    pushq        %r10
    pushq        %r11
    pushq        %r12
    pushq        %r13
    pushq        %r14
    pushq        %r15
.endm
/// Macro to restore syscall state, companion to state_save
.macro state_restore
    popq         %r15
    popq         %r14
    popq         %r13
    popq         %r12
    popq         %r11
    popq         %r10
    popq         %r9
    popq         %rbp
    popq         %rcx
    popq         %rbx
.endm

/**
 * The syscall number is passed in %ax; the high 48 bits of %rax are available for the syscall to
 * use as flags. On return, %rax contains the status code.
 *
 * Syscalls may have up to 4 arguments, passed in the %rdi, %rsi, %rdx and %r8 registers,
 * respectively.
 *
 * The SYSCALL in user mode will have set RCX to the return address and R11 to the flags to restore
 * on return. We'll return with the value of R9 set into the stack pointer.
 */
.global arch_syscall_entry
.align 0x40 // align to cache line
arch_syscall_entry:
    // acquire the per-CPU information and get stack
    swapgs
    movq        %gs:PROCI_OFF_SYSCALL_STACK, %rsp

    sti

    // save registers
    state_save
    xor         %rbp, %rbp

    // build the argument struct
    pushq       %r8
    pushq       %rdx
    pushq       %rsi
    pushq       %rdi

    // invoke the hadler
    movq        %rax, %rdi
    movq        %rsp, %rsi

    call        arch_syscall_handle
    add         $32, %rsp

    // restore state and exit
    state_restore
    jmp         arch_syscall_exit

/**
 * Common return handler for syscalls.
 *
 * At the time this is called, the registers should have been restored to the state they were in
 * at the time the syscall was made. We just have to restore %gs base, the user's stack, and then
 * return back to userland.
 *
 * The instruction will fault if we try to return to a page marked as supervisor-only access, or
 * use a stack that's not mapped for that process.
 */
arch_syscall_exit:
    // restore gs
    cli
    swapgs

    // return from syscall: R11 and RCX are the SYSCALL values
    sysretq

