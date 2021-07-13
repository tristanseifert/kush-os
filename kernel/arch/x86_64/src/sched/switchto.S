#define ASM_FILE
#include "ThreadState.h"
#include "gdt.h"

.global amd64_switchto
.global amd64_switchto_save
.global amd64_ring3_return
.global amd64_dpc_stub

/**
 * Performs a context switch to a thread; context is NOT saved. The thread to switch to should be
 * pointed to by %rdi.
 */
amd64_switchto:

/**
 * Common context switch restoration function; %rdi should point to a ThreadState struct, with a
 * correctly formed stack frame that we can return to.
 */
amd64_switchto_common:
    // restore FPU state
    movb        TS_OFF_FPU_SHOULD_RESTORE(%rdi), %al
    test        %al, %al
    jz          1f

    movq        TS_OFF_FPU_STATE_PTR(%rdi), %rcx
    mov         $0xFFFFFFFF, %eax
    mov         $0xFFFFFFFF, %edx

amd64_switchto_common_xrstor:
    xrstor64    (%rcx)

1:
    // switch back to the appropriate stack
    mov         TS_OFF_STACKTOP(%rdi), %rsp

amd64_switchto_common_stacked:
    // restore registers and CPU state
    lea         TS_OFF_REGS(%rdi), %rdi

    mov           0(%rdi), %r15
    mov           8(%rdi), %r14
    mov          16(%rdi), %r13
    mov          24(%rdi), %r12
    mov          32(%rdi), %r11
    mov          40(%rdi), %r10
    mov          48(%rdi), %r9
    mov          56(%rdi), %r8
    mov          72(%rdi), %rsi
    mov          80(%rdi), %rbp
    mov          88(%rdi), %rbx
    mov          96(%rdi), %rdx
    mov         104(%rdi), %rcx
    mov         112(%rdi), %rax

    // restore flags and overwrite rdi
    push        120(%rdi)
    mov          64(%rdi), %rdi
    popfq

    // return to caller
    ret



/**
 * Performs a context switch, saving the current thread state.
 *
 * %rdi: from task, %rsi: to task
 */
amd64_switchto_save:
    // save stack pointer
    mov         %rsp, TS_OFF_STACKTOP(%rdi)

    // back up %r15 first so we can set it to the reg base
    mov         %r15, TS_OFF_REGS+0(%rdi)
    lea         TS_OFF_REGS(%rdi), %r15

    // save general purpose regs
    mov         %r14,   8(%r15)
    mov         %r13,  16(%r15)
    mov         %r12,  24(%r15)
    mov         %r11,  32(%r15)
    mov         %r10,  40(%r15)
    mov         %r9,   48(%r15)
    mov         %r8,   56(%r15)
    mov         %rdi,  64(%r15)
    mov         %rsi,  72(%r15)
    mov         %rbp,  80(%r15)
    mov         %rbx,  88(%r15)
    mov         %rdx,  96(%r15)
    mov         %rcx, 104(%r15)
    mov         %rax, 112(%r15)

    pushfq
    pop         120(%r15)

    // save floating point state; set all bits (since it's ANDed against XCR0)
    movq        TS_OFF_FPU_STATE_PTR(%rdi), %rcx
    mov         $0xFFFFFFFF, %eax
    mov         $0xFFFFFFFF, %edx

amd64_switchto_save_xsave:
    xsave64     (%rcx)

    movb        $1, TS_OFF_FPU_SHOULD_RESTORE(%rdi)

    // the previous thread's state has been saved. switch to the next thread
    mov         %rsi, %rdi
    jmp         amd64_switchto_common

/**
 * Builds a stack frame suitable for use with IRET to return to ring 3. Additionally, all of the
 * general purpose registers are cleared.
 *
 * %rdi: Program counter
 * %rsi: Stack pointer
 * %rdx: Argument
 */
amd64_ring3_return:
    // to prevent interruptions
    cli

    // set up stack frame
    push        %rbp
    mov         %rsp, %rbp

    // the stack we're returning with
    pushq       $GDT_USER_DATA_SEG|0x3
    pushq       %rsi

    // flags: always enable interrupts
    xor         %rax, %rax
    or          $0x002, %rax // reserved bit, always 1
    or          $0x200, %rax
    push        %rax

    // program counter
    pushq       $GDT_USER_CODE_SEG|0x3
    pushq       %rdi

    // restore the argument
    mov         %rdx, %rdi

    // clear registers
    xor         %rax, %rax
    xor         %rbx, %rbx
    xor         %rcx, %rcx
    xor         %rdx, %rdx
    xor         %rbp, %rbp
    xor         %rsi, %rsi
    xor         %r8, %r8
    xor         %r9, %r9
    xor         %r10, %r10
    xor         %r11, %r11
    xor         %r12, %r12
    xor         %r13, %r13
    xor         %r14, %r14
    xor         %r15, %r15

    // return, and ensure we swap back to the userspace %gs base
    iretq

