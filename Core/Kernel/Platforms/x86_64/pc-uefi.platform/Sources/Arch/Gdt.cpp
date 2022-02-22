#include "Gdt.h"
#include "GdtTypes.h"

#include <Logging/Console.h>
#include <Runtime/String.h>

using namespace Platform::Amd64Uefi;

GdtDescriptor Gdt::gGdt[kGdtSize] __attribute__((aligned(64)));
size_t Gdt::gNextTss{0};

/// TSS for the bootstrap processor
static Tss gBspTss;
/// Interrupt stacks for the bootstrap processor
static __attribute__((aligned(64))) uintptr_t gBspIrqStacks[7][Gdt::kIrqStackSize];

/**
 * Initializes the system's GDT.
 *
 * We'll configure the null entry, the code/data segments, and set up the first TSS for this
 * processor (the bootstrap processor, or BSP) with its interrupt stacks.
 *
 * @remark Since entries are 64 bit, all TSS entries and friends are 64-bit entries that need two
 *         slots.
 */
void Gdt::Init() {
    memset(gGdt, 0, sizeof(GdtDescriptor) * kGdtSize);

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
        const auto stack = reinterpret_cast<uintptr_t>(&gBspIrqStacks[i]) 
            + (kIrqStackSize * sizeof(uintptr_t));

        gBspTss.ist[i].low = stack & 0xFFFFFFFF;
        gBspTss.ist[i].high = stack >> 32ULL;
    }

    InstallTss(0, &gBspTss);

    // Load the GDT into the processor and activate TSS
    Load();
    ActivateTask(0);
}

/**
 * Sets a GDT entry.
 * 
 * flags -> access byte; gran -> granularity byte (high bit)
 */
void Gdt::Set32(const size_t num, const uint32_t base, const uint32_t limit, const uint8_t flags,
        const uint8_t gran) {
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

    if(kLogSet) Kernel::Console::Debug("GDT32 %4x: %08x", num, *(uint32_t *) &gGdt[num]);
}

/**
 * Sets a 64-bit GDT entry.
 */
void Gdt::Set64(const size_t num, const uintptr_t base, const uint32_t limit, const uint8_t flags,
        const uint8_t granularity) {
    // ensure this is past the 32-bit descriptors
    REQUIRE(num >= (GDT_USER_CODE64_SEG / 8), "%u-bit GDT index out of range: %u", 64, num);

    // build the descriptor
    GdtDescriptor64 desc;
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

    if(kLogSet) Kernel::Console::Debug("GDT64 %4x: %016llx", num, *(uint64_t *) &desc);
}

/**
 * Activates a TSS based on its index..
 */
void Gdt::ActivateTask(const size_t task) {
    const uint16_t sel = ((task * 2) * 0x8) + GDT_FIRST_TSS;
    asm volatile("ltr %0" : : "r" (sel) : "memory");
}


/**
 * Load the GDT into the processor.
 *
 * @param numTss Number of TSS that have been allocated
 */
void Gdt::Load(const size_t numTss) {
    REQUIRE(numTss && (numTss < kTssSlots), "invalid number of TSS %zu", numTss);

    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((__packed__)) GDTR;

    // each TSS slot occupies 16 bytes
    GDTR.length = (GDT_FIRST_TSS + (numTss * 16)) - 1;
    GDTR.base = reinterpret_cast<uintptr_t>(&gGdt);
    asm volatile("lgdt (%0)" : : "r"(&GDTR) : "memory");

    if(kLogLoad) Kernel::Console::Debug("Load GDT %p len %u", GDTR.base, GDTR.length);

    // flush stale GDT entries
    Gdt::Flush();
}

/**
 * Initializes a TSS.
 *
 * @note This does _not_ allocate interrupt stacks. All 7 interrupt stacks should be allocated and
 *       stored in the TSS after this call.
 */
void Gdt::InitTss(Tss *tss) {
    memset(tss, 0, sizeof(Tss));

    // no IO map
    tss->ioMap = (sizeof(Tss) - 1);
}

/**
 * Installs the given TSS.
 *
 * @param i TSS index slot
 * @param tss The TSS object to install
 */
void Gdt::InstallTss(const size_t i, Tss *tss) {
    Set64((i * 2) + (GDT_FIRST_TSS / 8), reinterpret_cast<uintptr_t>(tss),
            sizeof(Tss) - 1, 0x89, 0);
}
