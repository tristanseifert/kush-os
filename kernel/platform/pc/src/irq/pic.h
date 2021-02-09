#ifndef PLATFORM_PC_IRQ_PIC_H
#define PLATFORM_PC_IRQ_PIC_H

#include <stdint.h>

/**
 * Initializes the 8259 PIC.
 *
 * We'll remap all PIC interrupts to 0x20-0x2F.
 */
void pic_init();

void pic_irq_eoi(const uint8_t irq);
void pic_irq_disable();

#endif
