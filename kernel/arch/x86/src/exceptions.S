.section .text

/// divide-by-zero
.globl  x86_exception_div0
x86_exception_div0:
    cli
    // push error code and interrupt number
    pushl       $0x00
    pushl       $0x00
    jmp         error_common_stub

/// debug exception
.globl x86_exception_debug
x86_exception_debug:
    cli
    pushl       $0x00
    pushl       $0x01
    jmp         error_common_stub

/// non-maskable interrupt
.globl x86_exception_nmi
x86_exception_nmi:
    cli
    pushl       $0x00
    pushl       $0x02
    jmp         error_common_stub

/// breakpoint handler
.globl x86_exception_breakpoint
x86_exception_breakpoint:
    cli
    pushl       $0x00
    pushl       $0x03
    jmp         error_common_stub

/// overflow
.globl x86_exception_overflow
x86_exception_overflow:
    cli
    pushl       $0x00
    pushl       $0x04
    jmp         error_common_stub

/// bound range exceeded
.globl x86_exception_bounds
x86_exception_bounds:
    cli
    pushl       $0x00
    pushl       $0x05
    jmp         error_common_stub

/// invalid opcode
.globl x86_exception_invalid_instruction
x86_exception_invalid_instruction:
    cli
    pushl       $0x00
    pushl       $0x06
    jmp         error_common_stub

/// device not available; called for FPU instructions if they're unavailable/disabled
.globl x86_exception_device_unavailable
x86_exception_device_unavailable:
    cli
    pushl       $0x00
    pushl       $0x07
    jmp         error_common_stub

/// double fault
.globl x86_exception_double_fault
x86_exception_double_fault:
    cli
    pushl       $0x08
    jmp         error_common_stub

/// Invalid TSS
.globl x86_exception_tss_invalid
x86_exception_tss_invalid:
    cli
    pushl       $0x0A
    jmp         error_common_stub

/// Segment not present
.globl x86_exception_segment_missing
x86_exception_segment_missing:
    cli
    pushl       $0x0B
    jmp         error_common_stub

/// Invalid stack segment
.globl x86_exception_ss_invalid
x86_exception_ss_invalid:
    cli
    pushl       $0x0C
    jmp         error_common_stub

/// General protection fault
.globl x86_exception_gpf
x86_exception_gpf:
    cli
    pushl       $0x0D
    jmp         error_common_stub

/// x87 floating point exception
.globl x86_exception_float
x86_exception_float:
    cli
    pushl       $0x00
    pushl       $0x0F
    jmp         error_common_stub

/// Alignment check
.globl x86_exception_alignment_check
x86_exception_alignment_check:
    cli
    pushl       $0x10
    jmp         error_common_stub

/// Machine check
.globl x86_exception_machine_check
x86_exception_machine_check:
    cli
    pushl       $0x00
    pushl       $0x11
    jmp         error_common_stub

/// SIMD floating point exception
.globl x86_exception_simd
x86_exception_simd:
    cli
    pushl       $0x00
    pushl       $0x12
    jmp         error_common_stub

/// Virtualization exception
.globl x86_exception_virtualization
x86_exception_virtualization:
    cli
    pushl       $0x00
    pushl       $0x13
    jmp         error_common_stub


.extern x86_handle_pagefault
/**
 * Page fault handler
 */
.globl x86_exception_pagefault
x86_exception_pagefault:
    // disable IRQs and push the interrupt number, and all regs
    cli
    pushl       $0x0E
    pusha

    // save data segment
    mov         %ds, %ax
    push        %eax

    // restore kernel data segment descriptor
    mov         $0x10, %ax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs

    // invoke page fault handler
    call        x86_handle_pagefault

    // reload the original data segment fault descriptor
    pop         %eax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs

    # restore registers, clear the pushed error code and ISR numbe 
    popa
    add         $0x8, %esp

    # re-enable interrupts
    sti
    iret

/*
* BSoD handler
*/
error_common_stub:
    // save register state, original data segment
    pusha

    mov         %ds, %ax
    push        %eax

    // reset to kernel data segment
    mov         $0x10, %ax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs

//    call	sys_restore_kern_pagetable
//    call 	error_handler

    // restore original data segment and registers
    pop         %eax
    mov         %ax, %ds
    mov         %ax, %es
    mov         %ax, %fs
    mov         %ax, %gs

    popa
    add         $0x8, %esp

    // re-enable interrupts
    sti
    iret

