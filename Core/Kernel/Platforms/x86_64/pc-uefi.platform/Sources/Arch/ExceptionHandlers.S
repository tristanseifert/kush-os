#define ASM_FILE
#include "ExceptionHandlers.h"

.section .text

/// divide-by-zero
.globl  amd64_exception_div0
amd64_exception_div0:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_DIVIDE
    jmp         ErrorCommonStub

/// debug exception
.globl amd64_exception_debug
amd64_exception_debug:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_DEBUG
    jmp         ErrorCommonStub

/// non-maskable interrupt
.globl amd64_exception_nmi
amd64_exception_nmi:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_NMI
    jmp         ErrorCommonStub

/// breakpoint handler
.globl amd64_exception_breakpoint
amd64_exception_breakpoint:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_BREAKPOINT
    jmp         ErrorCommonStub

/// overflow
.globl amd64_exception_overflow
amd64_exception_overflow:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_OVERFLOW
    jmp         ErrorCommonStub

/// bound range exceeded
.globl amd64_exception_bounds
amd64_exception_bounds:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_BOUNDS
    jmp         ErrorCommonStub

/// invalid opcode
.globl amd64_exception_invalid_instruction
amd64_exception_invalid_instruction:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_ILLEGAL_OPCODE
    jmp         ErrorCommonStub

/// device not available; called for FPU instructions if they're unavailable/disabled
.globl amd64_exception_device_unavailable
amd64_exception_device_unavailable:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_DEVICE_UNAVAIL
    jmp         ErrorCommonStub

/// double fault
.globl amd64_exception_double_fault
amd64_exception_double_fault:
    cli
    pushq       $AMD64_EXC_DOUBLE_FAULT
    jmp         ErrorCommonStub

/// Invalid TSS
.globl amd64_exception_tss_invalid
amd64_exception_tss_invalid:
    cli
    pushq       $AMD64_EXC_INVALID_TSS
    jmp         ErrorCommonStub

/// Segment not present
.globl amd64_exception_segment_missing
amd64_exception_segment_missing:
    cli
    pushq       $AMD64_EXC_SEGMENT_NP
    jmp         ErrorCommonStub

/// Invalid stack segment
.globl amd64_exception_ss_invalid
amd64_exception_ss_invalid:
    cli
    pushq       $AMD64_EXC_SS
    jmp         ErrorCommonStub

/// General protection fault
.globl amd64_exception_gpf
amd64_exception_gpf:
    cli
    pushq       $AMD64_EXC_GPF
    jmp         ErrorCommonStub

/// x87 floating point exception
.globl amd64_exception_float
amd64_exception_float:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_FP
    jmp         ErrorCommonStub

/// Alignment check
.globl amd64_exception_alignment_check
amd64_exception_alignment_check:
    cli
    pushq       $AMD64_EXC_ALIGNMENT
    jmp         ErrorCommonStub

/// Machine check
.globl amd64_exception_machine_check
amd64_exception_machine_check:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_MCE
    jmp         ErrorCommonStub

/// SIMD floating point exception
.globl amd64_exception_simd
amd64_exception_simd:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_SIMD_FP
    jmp         ErrorCommonStub

/// Virtualization exception
.globl amd64_exception_virtualization
amd64_exception_virtualization:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_VIRT
    jmp         ErrorCommonStub

/**
 * Common handler for exceptions; this prepares a function invocation into the platform specific
 * exception dispatcher
 */
ErrorCommonStub:
    // save registers
    pushq       %r15
    pushq       %r14
    pushq       %r13
    pushq       %r12
    pushq       %r11
    pushq       %r10
    pushq       %r9
    pushq       %r8
    pushq       %rbp
    pushq       %rdi
    pushq       %rsi
    pushq       %rdx
    pushq       %rcx
    pushq       %rbx
    pushq       %rax

    // jump to dispatcher
    xor         %rbp, %rbp
    mov         %rsp, %rdi
    call        _ZN8Platform9Amd64Uefi17ExceptionHandlers6HandleERNS0_9Processor4RegsE

    // restore registers
    popq        %rax
    popq        %rbx
    popq        %rcx
    popq        %rdx
    popq        %rsi
    popq        %rdi
    popq        %rbp
    popq        %r8
    popq        %r9
    popq        %r10
    popq        %r11
    popq        %r12
    popq        %r13
    popq        %r14
    popq        %r15

    // clear status code, irq number off the stack and retrurn
    add         $0x10, %rsp
    iretq

/**
 * Handler for page fault exceptions; this will eventually call directly into the VM system's page
 * fault handler, but for now, treat it as any other exception.
 */
.global amd64_exception_pagefault
amd64_exception_pagefault:
    // disable IRQs and push the interrupt number, and all regs
    cli
    pushq       $AMD64_EXC_PAGING

    // save registers
    pushq       %r15
    pushq       %r14
    pushq       %r13
    pushq       %r12
    pushq       %r11
    pushq       %r10
    pushq       %r9
    pushq       %r8
    pushq       %rbp
    pushq       %rdi
    pushq       %rsi
    pushq       %rdx
    pushq       %rcx
    pushq       %rbx
    pushq       %rax

    // jump to dispatcher
    xor         %rbp, %rbp
    mov         %rsp, %rdi
    call        _ZN8Platform9Amd64Uefi17ExceptionHandlers6HandleERNS0_9Processor4RegsE

    // restore registers
    popq        %rax
    popq        %rbx
    popq        %rcx
    popq        %rdx
    popq        %rsi
    popq        %rdi
    popq        %rbp
    popq        %r8
    popq        %r9
    popq        %r10
    popq        %r11
    popq        %r12
    popq        %r13
    popq        %r14
    popq        %r15

    // clear status code, irq number off the stack and retrurn
    add         $0x10, %rsp
    iretq
