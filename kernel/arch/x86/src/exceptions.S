#define ASM_FILE
#include "gdt.h"
#include "exception_types.h"

.section .text

/// divide-by-zero
.globl  x86_exception_div0
x86_exception_div0:
    cli
    // push error code and interrupt number
    pushl       $0x00
    pushl       $X86_EXC_DIVIDE
    jmp         error_common_stub

/// debug exception
.globl x86_exception_debug
x86_exception_debug:
    cli
    pushl       $0x00
    pushl       $X86_EXC_DEBUG
    jmp         error_common_stub

/// non-maskable interrupt
.globl x86_exception_nmi
x86_exception_nmi:
    cli
    pushl       $0x00
    pushl       $X86_EXC_NMI
    jmp         error_common_stub

/// breakpoint handler
.globl x86_exception_breakpoint
x86_exception_breakpoint:
    cli
    pushl       $0x00
    pushl       $X86_EXC_BREAKPOINT
    jmp         error_common_stub

/// overflow
.globl x86_exception_overflow
x86_exception_overflow:
    cli
    pushl       $0x00
    pushl       $X86_EXC_OVERFLOW
    jmp         error_common_stub

/// bound range exceeded
.globl x86_exception_bounds
x86_exception_bounds:
    cli
    pushl       $0x00
    pushl       $X86_EXC_BOUNDS
    jmp         error_common_stub

/// invalid opcode
.globl x86_exception_invalid_instruction
x86_exception_invalid_instruction:
    cli
    pushl       $0x00
    pushl       $X86_EXC_ILLEGAL_OPCODE
    jmp         error_common_stub

/// device not available; called for FPU instructions if they're unavailable/disabled
.globl x86_exception_device_unavailable
x86_exception_device_unavailable:
    cli
    pushl       $0x00
    pushl       $X86_EXC_DEVICE_UNAVAIL
    jmp         error_common_stub

/// double fault
.globl x86_exception_double_fault
x86_exception_double_fault:
    cli
    pushl       $X86_EXC_DOUBLE_FAULT
    jmp         error_common_stub

/// Invalid TSS
.globl x86_exception_tss_invalid
x86_exception_tss_invalid:
    cli
    pushl       $X86_EXC_INVALID_TSS
    jmp         error_common_stub

/// Segment not present
.globl x86_exception_segment_missing
x86_exception_segment_missing:
    cli
    pushl       $X86_EXC_SEGMENT_NP
    jmp         error_common_stub

/// Invalid stack segment
.globl x86_exception_ss_invalid
x86_exception_ss_invalid:
    cli
    pushl       $X86_EXC_SS
    jmp         error_common_stub

/// General protection fault
.globl x86_exception_gpf
x86_exception_gpf:
    cli
    pushl       $X86_EXC_GPF
    jmp         error_common_stub

/// x87 floating point exception
.globl x86_exception_float
x86_exception_float:
    cli
    pushl       $0x00
    pushl       $X86_EXC_FP
    jmp         error_common_stub

/// Alignment check
.globl x86_exception_alignment_check
x86_exception_alignment_check:
    cli
    pushl       $X86_EXC_ALIGNMENT
    jmp         error_common_stub

/// Machine check
.globl x86_exception_machine_check
x86_exception_machine_check:
    cli
    pushl       $0x00
    pushl       $X86_EXC_MCE
    jmp         error_common_stub

/// SIMD floating point exception
.globl x86_exception_simd
x86_exception_simd:
    cli
    pushl       $0x00
    pushl       $X86_EXC_SIMD_FP
    jmp         error_common_stub

/// Virtualization exception
.globl x86_exception_virtualization
x86_exception_virtualization:
    cli
    pushl       $0x00
    pushl       $X86_EXC_VIRT
    jmp         error_common_stub


.extern x86_handle_pagefault
/**
 * Page fault handler
 */
.globl x86_exception_pagefault
x86_exception_pagefault:
    // disable IRQs and push the interrupt number, and all regs
    cli
    pushl       $X86_EXC_PAGING
    pusha

    // save segment registers
    push        %ds
    push        %es
    push        %fs
    push        %gs

    // restore kernel data segment descriptor
    mov         $GDT_KERN_DATA_SEG, %ax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs

    // set up a NULL stack frame, so stack unwinding can stop here
    xor         %ebp, %ebp

    // invoke page fault handler
    call        x86_handle_pagefault

    // reload the original data segment fault descriptor
    pop         %gs
    pop         %fs
    pop         %es
    pop         %ds

    // restore registers, clear the pushed error code and ISR number
    popa
    add         $0x8, %esp

    // return; this will restore the interrupt flag too
    iret

.extern x86_handle_exception
/*
* BSoD handler
*/
error_common_stub:
    // save register state, original data segment
    pusha

    push        %ds
    push        %es
    push        %fs
    push        %gs

    // reset to kernel data segment
    mov         $GDT_KERN_DATA_SEG, %ax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs

    // set up a NULL stack frame, so stack unwinding can stop here
    xor         %ebp, %ebp
    call        x86_handle_exception

    // restore original data segment and registers
    pop         %gs
    pop         %fs
    pop         %es
    pop         %ds

    popa
    add         $0x8, %esp

    // return; this will restore the interrupt flag too
    iret

