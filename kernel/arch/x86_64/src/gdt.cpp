#include "gdt.h"

#include <mem/StackPool.h>

#include <log.h>
#include <string.h>
#include <arch/spinlock.h>

using namespace arch;

/// static GDT allocation
gdt_descriptor_t Gdt::gGdt[Gdt::kGdtSize] __attribute__((aligned(64)));

/// TSS for bootstrap processor
static amd64_tss_t gBspTss;

/// Size of the IRQ stack (in 8 byte units)
constexpr static const size_t kIrqStackSz = 256;
/// Interrupt stacks for the bootstrap processor
static __attribute__((aligned(64))) uintptr_t gBspIrqStacks[7][kIrqStackSz];

bool Gdt::gLogLoad = false;

/**
 * Loads the task register with the TSS in the given descriptor.
 */
/*static inline void tss_load(uint16_t sel) {
    asm volatile("ltr %0" : : "r" (sel));
}*/

/**
 * Set up the GDT; the null entry, as well as the code/data segments are set up.
 *
 * Additionally, the first TSS is allocated for the bootstrap processor.
 *
 * Due to 64-bit mode, all TSS entries and beyond are treated as 64-bit GDT entries. This means
 * that each entry actually takes up TWO entries in the table.
 */
void Gdt::Init() {
    // clear its memory first
    memset(gGdt, 0, sizeof(gdt_descriptor_t) * kGdtSize);

    // Kernel code segment and data segments: L (flag bit 5) indicates an x86_64 code descriptor
    Set32((GDT_KERN_CODE_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0x9A, 0xAF);
    Set32((GDT_KERN_DATA_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0x92, 0xCF); // should be 0x4F?

    // User code and data segments
    Set32((GDT_USER_CODE_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xFA, 0xAF);
    Set32((GDT_USER_CODE64_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xFA, 0xAF);

    Set32((GDT_USER_DATA_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xF2, 0xCF);

    // set up the first TSS
    InitTss(&gBspTss);

    for(size_t i = 0; i < 7; i++) {
        const auto stack = reinterpret_cast<uintptr_t>(&gBspIrqStacks[i]) + (kIrqStackSz * sizeof(uintptr_t));

        gBspTss.ist[i].low = stack & 0xFFFFFFFF;
        gBspTss.ist[i].high = stack >> 32ULL;
    }

    InstallTss(0, &gBspTss);

    // Load the GDT into the processor and activate TSS
    Load();
    ActivateTask(0);
}

/**
 * Activates a task.
 */
void Gdt::ActivateTask(const size_t task) {
    const uint16_t sel = ((task * 2) * 8) + GDT_FIRST_TSS;

    if(gLogLoad) log("Load task: %04x", sel);
    asm volatile("ltr %0" : : "r" (sel) : "memory");
}


/**
 * Sets a GDT entry.
 * 
 * flags -> access byte; gran -> granularity byte (high bit)
 */
void Gdt::Set32(const size_t num, const uint32_t base, const uint32_t limit, const uint8_t flags, const uint8_t gran) {
    // validate inputs
    REQUIRE(num <= (GDT_USER_CODE64_SEG / 8), "%u-bit GDT index out of range: %u", 32, num);

    // write it
    gGdt[num].base_low = (base & 0xFFFF);
    gGdt[num].base_middle = (base >> 16) & 0xFF;
    gGdt[num].base_high = (base >> 24) & 0xFF;

    gGdt[num].limit_low = (limit & 0xFFFF);
    gGdt[num].granularity = (limit >> 16) & 0x0F;

    gGdt[num].granularity |= gran & 0xF0;
    gGdt[num].access = flags;
}

/**
 * Sets a 64-bit GDT entry.
 */
void Gdt::Set64(const size_t num, const uintptr_t base, const uint32_t limit, const uint8_t flags, const uint8_t granularity) {
    // ensure this is past the 32-bit descriptors
    REQUIRE(num >= (GDT_USER_CODE64_SEG / 8), "%u-bit GDT index out of range: %u", 64, num);

    // build the descriptor
    gdt_descriptor64_t desc;
    memset(&desc, 0, sizeof(desc));

    desc.limit0 = (limit & 0xFFFF);
    desc.granularityLimit = (limit >> 16) & 0x0F;

    desc.base0 = (base & 0xFFFF);
    desc.base1 = (base >> 16) & 0xFF;
    desc.base2 = (base >> 24) & 0xFF;
    desc.base3 = (base >> 32ULL);

    desc.typeFlags = flags;

    // should only be 0b10010000 set
    desc.granularityLimit |= (granularity & 0xF0);

    // copy it in
    memcpy(&gGdt[num], &desc, sizeof(desc));
}

/**
 * Load the GDT into the processor.
 */
void Gdt::Load(const size_t numTss) {
    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((__packed__)) GDTR;

    // each TSS slot occupies 16 bytes
    GDTR.length = (GDT_FIRST_TSS + (numTss * 16)) - 1;
    GDTR.base = reinterpret_cast<uintptr_t>(&gGdt);
    asm volatile("lgdt (%0)" : : "r"(&GDTR) : "memory");

    if(gLogLoad) log("Load GDT %p len %u", GDTR.base, GDTR.length);
}

/**
 * Initializes a TSS.
 *
 * @note This does _not_ allocate interrupt stacks. All 7 interrupt stacks should be allocated.
 */
void Gdt::InitTss(amd64_tss_t *tss) {
    memset(tss, 0, sizeof(amd64_tss_t));

    // no IO map
    tss->ioMap = (sizeof(amd64_tss_t) - 1) << 16;
}

/**
 * Installs the given TSS.
 *
 * @param i TSS index slot, this is typically the CPU ID.
 */
void Gdt::InstallTss(const size_t i, amd64_tss_t *tss) {
    Set64((i * 2) + (GDT_FIRST_TSS / 8), reinterpret_cast<uintptr_t>(tss),
            sizeof(amd64_tss_t) - 1, 0x89, 0);
}
