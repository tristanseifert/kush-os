#define ASM_FILE
#include "gdt.h"
#include "exception_types.h"

.section .text

/**
 * Swap the current GS and the value of the KernelGSBase MSR, if the code segment pushed on the
 * stack (this assumes it's called right at the start of an ISR) and will only perform the swap
 * if it's NOT the kernel code segment.
 *
 * Note that in the case of "super atomic" interrupts (debug, NMI, MCE, etc.) you will have to
 * instead read the MSR to make the determination on whether we need to swap GS or not, so this
 * macro cannot be used.
 *
 * First arg to this macro is the stack offset to the code segment.
 *
 * TODO: fix this
 */
.macro MaybeSwapGs off
/*    cmpq        $GDT_KERN_CODE_SEG, \off(%rsp)
    je          1f
    swapgs
1:*/
.endm

/// divide-by-zero
.globl  amd64_exception_div0
amd64_exception_div0:
    cli
    MaybeSwapGs 8
    // push error code and interrupt number
    pushq       $0x00
    pushq       $X86_EXC_DIVIDE
    jmp         error_common_stub

/// debug exception
.globl amd64_exception_debug
amd64_exception_debug:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_DEBUG
    jmp         error_common_stub

/// non-maskable interrupt
.globl amd64_exception_nmi
amd64_exception_nmi:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_NMI
    jmp         error_common_stub

/// breakpoint handler
.globl amd64_exception_breakpoint
amd64_exception_breakpoint:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_BREAKPOINT
    jmp         error_common_stub

/// overflow
.globl amd64_exception_overflow
amd64_exception_overflow:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_OVERFLOW
    jmp         error_common_stub

/// bound range exceeded
.globl amd64_exception_bounds
amd64_exception_bounds:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_BOUNDS
    jmp         error_common_stub

/// invalid opcode
.globl amd64_exception_invalid_instruction
amd64_exception_invalid_instruction:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_ILLEGAL_OPCODE
    jmp         error_common_stub

/// device not available; called for FPU instructions if they're unavailable/disabled
.globl amd64_exception_device_unavailable
amd64_exception_device_unavailable:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_DEVICE_UNAVAIL
    jmp         error_common_stub

/// double fault
.globl amd64_exception_double_fault
amd64_exception_double_fault:
    cli
    MaybeSwapGs 16
    pushq       $X86_EXC_DOUBLE_FAULT
    jmp         error_common_stub

/// Invalid TSS
.globl amd64_exception_tss_invalid
amd64_exception_tss_invalid:
    cli
    MaybeSwapGs 16
    pushq       $X86_EXC_INVALID_TSS
    jmp         error_common_stub

/// Segment not present
.globl amd64_exception_segment_missing
amd64_exception_segment_missing:
    cli
    MaybeSwapGs 16
    pushq       $X86_EXC_SEGMENT_NP
    jmp         error_common_stub

/// Invalid stack segment
.globl amd64_exception_ss_invalid
amd64_exception_ss_invalid:
    cli
    MaybeSwapGs 16
    pushq       $X86_EXC_SS
    jmp         error_common_stub

/// General protection fault
.globl amd64_exception_gpf
amd64_exception_gpf:
    cli
    MaybeSwapGs 16
    pushq       $X86_EXC_GPF
    jmp         error_common_stub

/// x87 floating point exception
.globl amd64_exception_float
amd64_exception_float:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_FP
    jmp         error_common_stub

/// Alignment check
.globl amd64_exception_alignment_check
amd64_exception_alignment_check:
    cli
    MaybeSwapGs 16
    pushq       $X86_EXC_ALIGNMENT
    jmp         error_common_stub

/// Machine check
.globl amd64_exception_machine_check
amd64_exception_machine_check:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_MCE
    jmp         error_common_stub

/// SIMD floating point exception
.globl amd64_exception_simd
amd64_exception_simd:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_SIMD_FP
    jmp         error_common_stub

/// Virtualization exception
.globl amd64_exception_virtualization
amd64_exception_virtualization:
    cli
    MaybeSwapGs 8
    pushq       $0x00
    pushq       $X86_EXC_VIRT
    jmp         error_common_stub


.extern amd64_handle_pagefault
/**
 * Page fault handler
 */
.globl amd64_exception_pagefault
amd64_exception_pagefault:
    // disable IRQs and push the interrupt number, and all regs
    cli
    MaybeSwapGs 16
    pushq       $X86_EXC_PAGING

    // save registers pls
    pushq        %rax
    pushq        %rcx
    pushq        %rdx
    pushq        %rbx
    pushq        %rbp
    pushq        %rsi
    pushq        %rdi
    pushq        %r8
    pushq        %r9
    pushq        %r10
    pushq        %r11
    pushq        %r12
    pushq        %r13
    pushq        %r14
    pushq        %r15

/*
    sub         $16, %rsp
    movdqu      %xmm7, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm6, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm5, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm4, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm3, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm2, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm1, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm0, (%rsp)
*/

    // set up a NULL stack frame, so stack unwinding can stop here
    xor         %rbp, %rbp

    // invoke page fault handler
    mov         %rsp, %rdi
    call        amd64_handle_pagefault

    // the XMM registers don't need to be restored
    //add         $(16*8), %rsp

    // restore registers
    popq         %r15
    popq         %r14
    popq         %r13
    popq         %r12
    popq         %r11
    popq         %r10
    popq         %r9
    popq         %r8
    popq         %rdi
    popq         %rsi
    popq         %rbp
    popq         %rbx
    popq         %rdx
    popq         %rcx
    popq         %rax

    // clear the error code and interrupt number
    add         $0x10, %rsp

    // return; this will restore the interrupt flag too
    iretq

.extern amd64_handle_exception
error_common_stub:
    // save registers
    pushq        %rax
    pushq        %rcx
    pushq        %rdx
    pushq        %rbx
    pushq        %rbp
    pushq        %rsi
    pushq        %rdi
    pushq        %r8
    pushq        %r9
    pushq        %r10
    pushq        %r11
    pushq        %r12
    pushq        %r13
    pushq        %r14
    pushq        %r15

/*
    sub         $16, %rsp
    movdqu      %xmm7, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm6, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm5, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm4, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm3, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm2, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm1, (%rsp)
    sub         $16, %rsp
    movdqu      %xmm0, (%rsp)
*/

    // set up a NULL stack frame, so stack unwinding can stop here
    xor         %rbp, %rbp
    mov         %rsp, %rdi
    call        amd64_handle_exception

    // the XMM registers don't need to be restored
    //add         $(16*8), %rsp

    // restore registers
    popq         %r15
    popq         %r14
    popq         %r13
    popq         %r12
    popq         %r11
    popq         %r10
    popq         %r9
    popq         %r8
    popq         %rdi
    popq         %rsi
    popq         %rbp
    popq         %rbx
    popq         %rdx
    popq         %rcx
    popq         %rax

    // clear the error code and interrupt number
    add         $0x10, %rsp

    // return; this will restore the interrupt flag too
    iretq

