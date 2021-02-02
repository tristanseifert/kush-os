#include "gdt.h"

#include <string.h>

// the descriptor table is allocated in BSS
static gdt_descriptor_t sys_gdt[16];
static gdt_task_gate_t sys_tss[GDT_NUM_TSS];


static void load_gdt(void *location);
extern void gdt_flush();


/*
 * Builds the Global Descriptor Table with the proper code/data segments, and
 * space for some task state segment descriptors.
 */
void gdt_init() {
    gdt_descriptor_t *gdt = (gdt_descriptor_t *) &sys_gdt;

    // Set up null entry
    memset(gdt, 0x00, sizeof(gdt_descriptor_t));

    // Kernel code segment and data segments
    gdt_set_entry((GDT_KERN_CODE_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_entry((GDT_KERN_DATA_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0x92, 0xCF);

    // User code and data segments
    gdt_set_entry((GDT_USER_CODE_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_entry((GDT_USER_DATA_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xF2, 0xCF);

    // Create the correct number of TSS descriptors
    for(int i = 0; i < GDT_NUM_TSS; i++) {
        gdt_set_entry(i+5, ((uint32_t) &sys_tss)+(i*sizeof(gdt_task_gate_t)), sizeof(gdt_task_gate_t), 0x89, 0x4F);
    }

    load_gdt(gdt);
}

/**
 * Sets a GDT entry.
 */
void gdt_set_entry(uint16_t num, uint32_t base, uint32_t limit, uint8_t flags, uint8_t gran) {
    gdt_descriptor_t *gdt = (gdt_descriptor_t *) &sys_gdt;

    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = flags;
}

/*
 * Installs the GDT.
 */
static void load_gdt(void *location) {
    struct {
        uint16_t length;
        uint32_t base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = (0x18 + (GDT_NUM_TSS * 0x08)) - 1;
    IDTR.base = (uint32_t) location;
    __asm__ volatile("lgdt (%0)" : : "p"(&IDTR));

    gdt_flush();
}