#ifndef ARCH_X86_IDT_H
#define ARCH_X86_IDT_H

#include <stdint.h>

/// IDT flags suitable for an ISR: present, DPL=0, 32-bit interrupt gate
#define IDT_FLAGS_ISR                   0x8E
/// IDT flags suitable for an exception/trap: present, DPL=0, 32-bit trap gate
#define IDT_FLAGS_TRAP                  0x8F

/// Describes an interrupt descriptor entry
typedef struct arch_idt_descriptor {
    /// ofset bits 0..15
    uint16_t offset_1;
    /// a code segment selector in GDT/LDT
    uint16_t selector;
    /// unused; set as 0
    uint8_t zero;
    /// type and attributes
    uint8_t flags;
    /// offset bits 16..32
    uint16_t offset_2;
} __attribute__((packed)) idt_entry_t;


void idt_init();
void idt_set_entry(uint8_t entry, uintptr_t function, uint8_t segment, uint8_t flags);

#endif
