#include "gdt.h"

#include <mem/StackPool.h>

#include <log.h>
#include <string.h>
#include <arch/spinlock.h>

// the descriptor table is allocated in BSS
constexpr static const size_t kGdtSize = 64;
static gdt_descriptor_t sys_gdt[kGdtSize];

static gdt_task_gate_t gTss[GDT_NUM_TSS];
static void *gTssStacks[GDT_NUM_TSS] = {nullptr};
static bool gTssLoaded[GDT_NUM_TSS] = {0};

// allocation map of TSS indices
constexpr static const size_t kNumTss = (kGdtSize - 8);
static bool gTssAllocated[kNumTss] = {0};
static uint8_t *gTssAllocatedBuf[kNumTss] = {nullptr};

constexpr static const uintptr_t kFirstAllocTss = (GDT_FIRST_TSS >> 3) + GDT_NUM_TSS;

DECLARE_SPINLOCK_S(gTssAllocatedLock);

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
    gdt_set_entry((GDT_USER_TLS_SEG >> 3), 0x00000000, 0xFFFFFFFF, 0xF2, 0xCF);

    // Create the correct number of TSS descriptors
    for(int i = 0; i < GDT_NUM_TSS; i++) {
        gdt_set_entry((GDT_FIRST_TSS >> 3) + i, ((uint32_t) &gTss[i]),
                      sizeof(gdt_task_gate_t)-1, 0x89, 0x4F);
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

/**
 * Updates the base address of the GDT entry corresponding to the %gs register, used for thread-
 * local storage. This update affects only the current processor, and only user mode.
 */
void gdt_update_tls_user(const uintptr_t base) {
    gdt_set_entry((GDT_USER_TLS_SEG >> 3), base, 0xFFFFFFFF, 0xF2, 0xCF);
}

/*
 * Installs the GDT.
 */
static void load_gdt(void *location) {
    struct {
        uint16_t length;
        uint32_t base;
    } __attribute__((__packed__)) IDTR;

    IDTR.length = (0x28 + (GDT_NUM_TSS * 8) + (kNumTss * 8)) - 1;
    IDTR.base = (uint32_t) location;
    asm volatile("lgdt (%0)" : : "r"(&IDTR));

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

        gTss[i].iomap = sizeof(gdt_task_gate_t);
    }
}

/**
 * Updates the TSS for the current processor to point to the specified stack.
 *
 * TODO: handle multiprocessor. we also just force clear the BUSY flag -- is this ok?
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

    // reset the BUSY flag of the TSS
    gdt_set_entry((GDT_FIRST_TSS >> 3) + tssIdx, ((uint32_t) &gTss[tssIdx]),
                      sizeof(gdt_task_gate_t), 0x89, 0x4F);

    tss_load(GDT_FIRST_TSS + (tssIdx * 8));
}



/**
 * Allocate the first free TSS entry.
 *
 * @return 0 if successful, error code otherwise.
 */
const int tss_allocate(uintptr_t &idx) {
    size_t allocated = 0;
    SPIN_LOCK_GUARD(gTssAllocatedLock);

    // find a free TSS
    for(size_t i = 0; i < kNumTss; i++) {
        if(!gTssAllocated[i]) {
            gTssAllocated[i] = true;
            allocated = i;
            goto beach;
        }
    }

    // if we get here, no free TSS slots
    return -1;

beach:;
    // initialize the TSS
    const auto size = sizeof(gdt_task_gate_t) + ((65536 / 8) + 1);
    auto buf = new uint8_t[size];

    memset(buf, 0, size);
    gTssAllocatedBuf[allocated] = buf;

    auto tss = reinterpret_cast<gdt_task_gate_t *>(buf);
    auto iopb = reinterpret_cast<uint8_t *>(buf + sizeof(gdt_task_gate_t));

    // set up segments
    tss->ss0 = GDT_KERN_DATA_SEG;
    tss->cs = GDT_KERN_CODE_SEG | 3;
    tss->ds = GDT_KERN_DATA_SEG | 3;
    tss->es = GDT_KERN_DATA_SEG | 3;
    tss->fs = GDT_KERN_DATA_SEG | 3;
    tss->gs = GDT_KERN_DATA_SEG | 3;
    tss->ss = GDT_KERN_DATA_SEG | 3;

    // the IO permission map follows directly after; no ports are allowed yet
    tss->iomap = sizeof(gdt_task_gate_t);
    memset(iopb, 0xFF, (65536 / 8) + 1);

    // update the GDT entry
    gdt_set_entry((kFirstAllocTss + allocated), reinterpret_cast<uintptr_t>(tss),
                      sizeof(gdt_task_gate_t) + (65536/8) - 1, 0x89, 0x4F);

    return 0;
}

/**
 * Releases the GDT entry for the specified TSS.
 */
void tss_release(const uintptr_t idx) {
    REQUIRE(idx < kNumTss, "invalid tss index: %u", idx);

    SPIN_LOCK_GUARD(gTssAllocatedLock);

    // clear the flags
    gTssAllocated[idx] = false;
    gdt_set_entry(kFirstAllocTss + idx, 0, 0, 0, 0);

    // free its memory
    delete[] gTssAllocatedBuf[idx];
    gTssAllocatedBuf[idx] = nullptr;
}

/**
 * Activates a TSS that was previously allocated.
 */
void tss_activate(const uintptr_t idx, const uintptr_t stackAddr) {
    REQUIRE(gTssAllocated[idx], "cannot activate unallocated TSS %u", idx);

    // update stack
    auto tss = reinterpret_cast<gdt_task_gate_t *>(gTssAllocatedBuf[idx]);
    tss->esp0 = stackAddr;

    // reload it (after clearing busy flag)
    gdt_set_entry((kFirstAllocTss + idx), reinterpret_cast<uintptr_t>(tss),
                      sizeof(gdt_task_gate_t) + (65536/8) - 1, 0x89, 0x4F);
    tss_load((kFirstAllocTss * 8) + (idx * 8));
}

/**
 * Writes into the IO permission bitmap.
 */
void tss_write_iopb(const uintptr_t idx, const uintptr_t portOffset, const uint8_t *inIopb,
        const uintptr_t iopbBits) {
    SPIN_LOCK_GUARD(gTssAllocatedLock);

    REQUIRE(idx < kNumTss, "invalid tss index: %u", idx);
    REQUIRE(gTssAllocated[idx], "cannot update IOPB of unallocated TSS %u", idx);

    // get the info on the TSS
    auto buf = gTssAllocatedBuf[idx];
    auto iopb = reinterpret_cast<uint8_t *>(buf + sizeof(gdt_task_gate_t));

    // read the input bitmap
    for(size_t i = 0; i < iopbBits; i++) {
        // test if the bit is set in the input
        const bool isSet = (inIopb[i / 8]) & (1 << (i % 8));

        // if the bit is set in input, clear it in output
        const uintptr_t outBit = portOffset + i;
        if(isSet) {
            iopb[outBit / 8] &= ~(1 << (outBit % 8));
        } else {
            iopb[outBit / 8] |= (1 << (outBit % 8));
        }
    }
}
