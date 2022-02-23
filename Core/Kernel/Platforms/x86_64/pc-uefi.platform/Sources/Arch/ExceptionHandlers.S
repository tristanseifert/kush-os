#define ASM_FILE
#include "ExceptionHandlers.h"

.section .text

/// divide-by-zero
.globl  amd64_exception_div0
.type amd64_exception_div0, function
amd64_exception_div0:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_DIVIDE
    jmp         Amd64CommonErrorHandler

/// debug exception
.globl amd64_exception_debug
.type amd64_exception_debug, function
amd64_exception_debug:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_DEBUG
    jmp         Amd64CommonErrorHandler

/// non-maskable interrupt
.globl amd64_exception_nmi
.type amd64_exception_nmi, function
amd64_exception_nmi:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_NMI
    jmp         Amd64CommonErrorHandler

/// breakpoint handler
.globl amd64_exception_breakpoint
.type amd64_exception_breakpoint, function
amd64_exception_breakpoint:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_BREAKPOINT
    jmp         Amd64CommonErrorHandler

/// overflow
.globl amd64_exception_overflow
.type amd64_exception_overflow, function
amd64_exception_overflow:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_OVERFLOW
    jmp         Amd64CommonErrorHandler

/// bound range exceeded
.globl amd64_exception_bounds
.type amd64_exception_bounds, function
amd64_exception_bounds:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_BOUNDS
    jmp         Amd64CommonErrorHandler

/// invalid opcode
.globl amd64_exception_invalid_instruction
.type amd64_exception_invalid_instruction, function
amd64_exception_invalid_instruction:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_ILLEGAL_OPCODE
    jmp         Amd64CommonErrorHandler

/// device not available; called for FPU instructions if they're unavailable/disabled
.globl amd64_exception_device_unavailable
.type amd64_exception_device_unavailable, function
amd64_exception_device_unavailable:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_DEVICE_UNAVAIL
    jmp         Amd64CommonErrorHandler

/// double fault
.globl amd64_exception_double_fault
.type amd64_exception_double_fault, function
amd64_exception_double_fault:
    cli
    pushq       $AMD64_EXC_DOUBLE_FAULT
    jmp         Amd64CommonErrorHandler

/// Invalid TSS
.globl amd64_exception_tss_invalid
.type amd64_exception_tss_invalid, function
amd64_exception_tss_invalid:
    cli
    pushq       $AMD64_EXC_INVALID_TSS
    jmp         Amd64CommonErrorHandler

/// Segment not present
.globl amd64_exception_segment_missing
.type amd64_exception_segment_missing, function
amd64_exception_segment_missing:
    cli
    pushq       $AMD64_EXC_SEGMENT_NP
    jmp         Amd64CommonErrorHandler

/// Invalid stack segment
.globl amd64_exception_ss_invalid
.type amd64_exception_ss_invalid, function
amd64_exception_ss_invalid:
    cli
    pushq       $AMD64_EXC_SS
    jmp         Amd64CommonErrorHandler

/// General protection fault
.globl amd64_exception_gpf
.type amd64_exception_gpf, function
amd64_exception_gpf:
    cli
    pushq       $AMD64_EXC_GPF
    jmp         Amd64CommonErrorHandler

/// x87 floating point exception
.globl amd64_exception_float
.type amd64_exception_float, function
amd64_exception_float:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_FP
    jmp         Amd64CommonErrorHandler

/// Alignment check
.globl amd64_exception_alignment_check
.type amd64_exception_alignment_check, function
amd64_exception_alignment_check:
    cli
    pushq       $AMD64_EXC_ALIGNMENT
    jmp         Amd64CommonErrorHandler

/// Machine check
.globl amd64_exception_machine_check
.type amd64_exception_machine_check, function
amd64_exception_machine_check:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_MCE
    jmp         Amd64CommonErrorHandler

/// SIMD floating point exception
.globl amd64_exception_simd
.type amd64_exception_simd, function
amd64_exception_simd:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_SIMD_FP
    jmp         Amd64CommonErrorHandler

/// Virtualization exception
.globl amd64_exception_virtualization
.type amd64_exception_virtualization, function
amd64_exception_virtualization:
    cli
    pushq       $0x00
    pushq       $AMD64_EXC_VIRT
    jmp         Amd64CommonErrorHandler

/**
 * Common handler for exceptions; this prepares a function invocation into the platform specific
 * exception dispatcher
 */
.type Amd64CommonErrorHandler, function
Amd64CommonErrorHandler:
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
.type amd64_exception_pagefault, function
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

