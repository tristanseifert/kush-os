#define ASM_FILE
#include "handlers.h"

.extern platform_isr_handle

/// Macro for saving processor state and setting the status code
.macro          isr_start type=0xFF
    // save general-purpose registers and set up stack frame
    pusha
    xor         %ebp, %ebp
    // then push the irq code
    pushl       $\type
.endm
/// Macro for restoring the processor's state at the end of an ISR
.macro          isr_end
    // ensure the status code we pushed earlier gets yeeted
    add         $0x4, %esp
    // restore registers
    popa
    // return from ISR
    iret
.endm

/// Generates an ISR with the given name and type code.
.macro          define_isr name, type
.global platform_isr_\name
platform_isr_\name:
    isr_start   \type
    call        platform_isr_handle
    isr_end
.endm

.global platform_isr_spurious_pic
/// ISR for handling spurious interrupts received from master PIC
platform_isr_spurious_pic:
    isr_start   ISR_SPURIOUS_PIC
    call        platform_isr_handle
    isr_end

.global platform_isr_spurious_pic2
/// handles spurious IRQs from slave PIC; we need to send EOI to master
platform_isr_spurious_pic2:
    isr_start   ISR_SPURIOUS_PIC
    call        platform_isr_handle

    // send an EOI
    mov         $0x20, %al
    outb        %al, $0x20

    isr_end

.global platform_isr_spurious_apic
/// ISR for handling spurious interrupts received from processor local APIC
platform_isr_spurious_apic:
    isr_start   ISR_SPURIOUS_APIC
    call        platform_isr_handle
    isr_end


// ISA interrupts
define_isr isa_0, ISR_ISA_0
define_isr isa_1, ISR_ISA_1
define_isr isa_2, ISR_ISA_2
define_isr isa_3, ISR_ISA_3
define_isr isa_4, ISR_ISA_4
define_isr isa_5, ISR_ISA_5
define_isr isa_6, ISR_ISA_6
define_isr isa_7, ISR_ISA_7
define_isr isa_8, ISR_ISA_8
define_isr isa_9, ISR_ISA_9
define_isr isa_10, ISR_ISA_10
define_isr isa_11, ISR_ISA_11
define_isr isa_12, ISR_ISA_12
define_isr isa_13, ISR_ISA_13
define_isr isa_14, ISR_ISA_14
define_isr isa_15, ISR_ISA_15

