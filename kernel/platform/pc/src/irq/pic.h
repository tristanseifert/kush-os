#ifndef PLATFORM_PC_IRQ_PIC_H
#define PLATFORM_PC_IRQ_PIC_H

#include <stdint.h>

/**
 * Initializes the 8259 PIC.
 *
 * We'll remap all PIC interrupts to 0x20-0x2F.
 */
void pic_init();

void pic_irq_set_mask(uint8_t IRQline);
void pic_irq_clear_mask(uint8_t IRQline);
uint16_t pic_irq_get_isr();
uint16_t pic_irq_get_irr();
void pic_irq_eoi(const uint8_t irq);
void pic_irq_disable();

#endif
