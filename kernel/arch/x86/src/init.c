#include <arch.h>

#include "gdt.h"
#include "idt.h"

/**
 * Performs architecture initialization.
 *
 * Specifically, we set up the descriptor tables (GDT) and interrupt table (IDT) for protected
 * mode operation.
 */
void arch_init() {
    gdt_init();
    idt_init();
}
