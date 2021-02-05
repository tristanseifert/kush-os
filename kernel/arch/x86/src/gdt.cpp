#include "gdt.h"

#include <mem/StackPool.h>

#include <log.h>
#include <string.h>

// the descriptor table is allocated in BSS
constexpr static const size_t kGdtSize = 32;
static gdt_descriptor_t sys_gdt[kGdtSize];

static gdt_task_gate_t gTss[GDT_NUM_TSS];
static void *gTssStacks[GDT_NUM_TSS];
static bool gTssLoaded[GDT_NUM_TSS];

/// loads a GDT
static void load_gdt(void *location);
/// external assembly routine to flush cached GDT entries
extern "C" void gdt_flush();

/**
 * Loads the task register with the TSS in the given descriptor.
 */
static inline void tss_load(uint16_t sel) {
    asm volatile("ltr %0" : : "r" (sel));
}

/*
 * Builds the Global Descriptor Table with the proper code/data segments, and
 * space for some task state segment descriptors.
 */
void gdt_init() {
    gdt_descriptor_t *gdt = (gdt_descriptor_t *) &sys_gdt;

    // Set up null entry
    memset(gdt, 0x00, sizeof(gdt_descriptor_t) * kGdtSize);

    // Kernel code segment and data segments
    gdt_set_entry((GDT_KERN_CODE_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_entry((GDT_KERN_DATA_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0x92, 0xCF);

    // User code and data segments
    gdt_set_entry((GDT_USER_CODE_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_entry((GDT_USER_DATA_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xF2, 0xCF);

    // Create the correct number of TSS descriptors
    for(int i = 0; i < GDT_NUM_TSS; i++) {
        gdt_set_entry((GDT_FIRST_TSS >> 3) + i, ((uint32_t) &gTss[i]),
                      sizeof(gdt_task_gate_t), 0x89, 0x4F);
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

    IDTR.length = (0x28 + (GDT_NUM_TSS * 8)) - 1;
    IDTR.base = (uint32_t) location;
    __asm__ volatile("lgdt (%0)" : : "p"(&IDTR));

    gdt_flush();
}

/**
 * Configures the task structures. This allocates stacks for them but not much more.
 */
void gdt_setup_tss() {
    for(int i = 0; i < GDT_NUM_TSS; i++) {
        // clear it
        memset(&gTss[i], 0, sizeof(gdt_task_gate_t));

        // allocate the stack page
        void *stack = mem::StackPool::get();
        REQUIRE(stack, "failed to allocate kernel stack for TSS");

        gTssStacks[i] = stack;
        gTssLoaded[i] = false;

        gTss[i].ss0 = GDT_KERN_DATA_SEG;
        gTss[i].esp0 = (uintptr_t) stack;

        // allow entry to kernel mode via this TSS
        gTss[i].cs = GDT_KERN_CODE_SEG | 3;

        gTss[i].ds = GDT_KERN_DATA_SEG | 3;
        gTss[i].es = GDT_KERN_DATA_SEG | 3;
        gTss[i].fs = GDT_KERN_DATA_SEG | 3;
        gTss[i].gs = GDT_KERN_DATA_SEG | 3;
        gTss[i].ss = GDT_KERN_DATA_SEG | 3;
    }
}

/**
 * Updates the TSS for the current processor to point to the specified stack.
 *
 * TODO: handle multiprocessor
 */
void tss_set_esp0(void *ptr) {
    const auto tssIdx = 0;

    // a null pointer indicates the per core stack
    if(!ptr) ptr = gTssStacks[tssIdx];
    // save us the work if the stack won't change (TSS loads could be slow)
    if(gTss[tssIdx].esp0 == (uintptr_t) ptr && gTssLoaded[tssIdx]) return;
    gTssLoaded[tssIdx] = true;

    // otherwise, update the TSS and then flush it
    gTss[tssIdx].esp0 = (uintptr_t) ptr;

    tss_load(GDT_FIRST_TSS + (tssIdx * 8));
}

