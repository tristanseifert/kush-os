#include "idt.h"
#include "gdt.h"
#include "exceptions.h"

#include <string.h>

static void load_idt(void *location, const uint16_t size);

// the descriptor table is allocated in BSS
static idt_entry_t sys_idt[256];


/*
 * Builds the Interrupt Descriptor Table at a fixed location in memory.
 */
void idt_init() {
    // clear the IDT and install dummy IRQ handlers
    idt_entry_t* idt = (idt_entry_t *) &sys_idt;
    memset(idt, 0, sizeof(idt_entry_t)*256);

    for(int i = 0; i < 256; i++) {
        // idt_set_entry(i, (uint32_t) sys_dummy_irq, GDT_KERN_CODE_SEG, 0x8E);
    }

    // install CPU exception handlers
    exception_install_handlers();

    // load the IDT into the processor
    load_idt((void *) idt, sizeof(idt_entry_t)*256);
}


/**
 * Sets the value of an IDT gate.
 */
void idt_set_entry(uint8_t entry, uintptr_t function, uint8_t segment, uint8_t flags) {
    idt_entry_t *ptr = (idt_entry_t *) &sys_idt;

    ptr[entry].offset_1 = function & 0xFFFF;
    ptr[entry].offset_2 = (function >> 0x10) & 0xFFFF;
    ptr[entry].selector = segment;
    ptr[entry].flags = flags; // OR with 0x60 for user level
    ptr[entry].zero = 0x00;
}

/**
 * Loads an interrupt descriptor table.
 */
static void load_idt(void *base, const uint16_t size) {
    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = size - 1;
    IDTR.base = (uint64_t) base;
    asm volatile("lidt (%0)": : "r"(&IDTR));
}
