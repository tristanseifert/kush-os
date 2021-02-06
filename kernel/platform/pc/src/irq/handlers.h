#ifndef PLATFORM_PC_IRQ_HANDLERS_H
#define PLATFORM_PC_IRQ_HANDLERS_H

/// ISR code for PIC-originated spurious interrupts
#define ISR_SPURIOUS_PIC                0x80
/// ISR code for APIC-originated spurious interrupts
#define ISR_SPURIOUS_APIC               0x81

/// ISA interrupt: programmable timer
#define ISR_ISA_0                       0x30
/// ISA interrupt: keyboard
#define ISR_ISA_1                       0x31
#define ISR_ISA_2                       0x32
/// ISA interrupt: COM2
#define ISR_ISA_3                       0x33
/// ISA interrupt: COM1
#define ISR_ISA_4                       0x34
/// ISA interrupt: LPT2 (if exists)
#define ISR_ISA_5                       0x35
/// ISA interrupt: floppy controller
#define ISR_ISA_6                       0x36
/// ISA interrupt: LPT1 (if enabled)
#define ISR_ISA_7                       0x37
/// ISA interrupt: CMOS clock alarm
#define ISR_ISA_8                       0x38
#define ISR_ISA_9                       0x39
#define ISR_ISA_10                      0x3A
#define ISR_ISA_11                      0x3B
/// ISA Interrupt: PS2 mouse
#define ISR_ISA_12                      0x3C
#define ISR_ISA_13                      0x3D
/// ISA interrupt: Primary ATA controller
#define ISR_ISA_14                      0x3E
/// ISA interrupt: Secondary ATA controller
#define ISR_ISA_15                      0x3F

#ifndef ASM_FILE
#include <stdint.h>

/// ISR handler called by assembly functions
extern "C" void platform_isr_handle(const uint32_t type);

#endif

#endif
