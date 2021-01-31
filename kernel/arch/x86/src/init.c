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

/**
 * x86 page size is always 4K. There's also support for 4M (2M in PAE mode) large pages.
 */
size_t arch_page_size() {
    return 4096;
}
