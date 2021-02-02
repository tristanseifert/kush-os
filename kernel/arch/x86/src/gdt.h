#ifndef ARCH_X86_GDT_H
#define ARCH_X86_GDT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define GDT_KERN_CODE_SEG       0x08
#define GDT_KERN_DATA_SEG       0x10
#define GDT_USER_CODE_SEG       0x18
#define GDT_USER_DATA_SEG       0x20

#define GDT_NUM_TSS             4

/// GDT entry type
typedef struct arch_gdt_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_descriptor_t;

/// Describes a task gate
typedef struct i386_task_gate {
    // High word ignored
    uint32_t backlink;

    // All 32 bits significant for ESP, high word ignored for SS
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;

    // All 32 bits are significant
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    // High word is ignored in all these
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;

    uint16_t trap;
    uint16_t iomap; // low word ignored
} __attribute__((packed)) gdt_task_gate_t;

void gdt_init();
void gdt_set_entry(uint16_t num, uint32_t base, uint32_t limit, uint8_t flags, uint8_t gran);

#ifdef __cplusplus
}
#endif
#endif