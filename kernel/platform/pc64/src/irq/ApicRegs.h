#ifndef PLATFORM_PC64_IRQ_APICREGS_H
#define PLATFORM_PC64_IRQ_APICREGS_H

#include <stdint.h>

/// APIC base address MSR number
#define IA32_APIC_BASE_MSR 0x1B
/// Processor is BSP flag
#define IA32_APIC_BASE_MSR_BSP 0x100 
/// APIC enable flag
#define IA32_APIC_BASE_MSR_ENABLE 0x800

/**
 * Contains the version (bits 7-0), the maximum LVT entry (bits 23-16) and whether EOI
 * broadcast suppression is supported (bit 24)
 */
constexpr static const uint32_t kApicRegVersion         = 0x030;

/**
 * Task priority register; write in this the lowest interrupt vector that you want to receive on
 * this processor.
 */
constexpr static const uint32_t kApicRegTaskPriority    = 0x080;

/**
 * End of interrupt register.
 *
 * This is write-only; you must write a '0' to it to end an interrupt.
 */
constexpr static const uint32_t kApicRegEndOfInt        = 0x0B0;

/**
 * Spurious interrupt and enable register
 */
constexpr static const uint32_t kApicRegSpurious        = 0x0F0;

/**
 * Low half of the interrupt command register
 */
constexpr static const uint32_t kApicRegInterruptCmdLow = 0x300;

/**
 * LVT entry for the core local timer
 */
constexpr static const uint32_t kApicRegLvtTimer        = 0x320;
/**
 * LVT entry for the local interrupt 0 pin.
 */
constexpr static const uint32_t kApicRegLvtLint0        = 0x350;
/**
 * LVT entry for the local interrupt 1 pin.
 */
constexpr static const uint32_t kApicRegLvtLint1        = 0x360;

/**
 * Timer initial count (all 32 bits are usable)
 */
constexpr static const uint32_t kApicRegTimerInitial    = 0x380;
/**
 * Timer current count register (read only)
 */
constexpr static const uint32_t kApicRegTimerCurrent    = 0x390;
/**
 * Timer divide configuration register. The upper 28 bits are reserved; the low 4 bits encode the
 * division ratio from the timer input clock to use for counting:
 *
 * - 0000: Divide by 2
 * - 0001: Divide by 4
 * - 0010: Divide by 8
 * - 0011: Divide by 16
 * - 1000: Divide by 32
 * - 1001: Divide by 64
 * - 1010: Divide by 128
 * - 1011: Divide by 1 (this is broken on many implementations)
 */
constexpr static const uint32_t kApicRegTimerDivide     = 0x3E0;


#endif
