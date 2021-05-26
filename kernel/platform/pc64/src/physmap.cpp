#include "physmap.h"

#include <bootboot.h>

#include <log.h>
#include <platform.h>

#include <vm/Map.h>

#include <arch.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" BOOTBOOT bootboot;
extern unsigned char environment[4096];

// XXX: we should pull this in from the x86_64 arch headers
namespace arch::vm {
int ResolvePml4Virt(const uintptr_t pml4, const uintptr_t virt, uintptr_t &phys);
}

using namespace platform;

extern size_t __kern_keep_start;
extern size_t __kern_code_start, __kern_code_end;
extern size_t __kern_data_start, __kern_data_size;
extern size_t __kern_bss_start, __kern_bss_size;
extern size_t __kern_keep_start, __kern_keep_end;

/// Physical base address of kernel text region
uintptr_t Physmap::gKernelTextPhys = 0xBADBADBEEF;
/// Physical base address of the kernel data region
uintptr_t Physmap::gKernelDataPhys = 0xBADBADBEEF;
/// Physical base address of the kernel zero initialized (bss) region
uintptr_t Physmap::gKernelBssPhys = 0xBADBADBEEF;

/// Physical base address of bootboot info
uintptr_t Physmap::gBootInfoPhys = 0xBADBADBEEF;
/// Physical base address of the bootboot environment block
uintptr_t Physmap::gBootEnvPhys = 0;

/// Maximum number of physical memory regions to allocate space for
constexpr static const size_t kMaxRegions = 16;

/// Total number of allocatable physical RAM regions
static size_t gNumPhysRegions = 0;
/// Info on each of the physical regions
static physmap_region_t gPhysRegions[kMaxRegions] = {
    {0, 0},
};

/// end address of the modules region
static uintptr_t gModulesEnd = 0;

/// log allocated physical regions
static bool gLogPhysRegions = false;

static void create_kernel_hole();



/**
 * Parse the multiboot structure for all of the memory in the machine.
 */
void Physmap::Init() {
    auto &boot = bootboot;

    // prepare our buffer
    gNumPhysRegions = 0;
    memclr(&gPhysRegions, sizeof(physmap_region_t) * kMaxRegions);

    // iterate over all entries
    const MMapEnt *mmap = nullptr;
    for(mmap = &boot.mmap; (const uint8_t *) mmap < (const uint8_t *) &boot + boot.size; mmap++) {
        const auto type = MMapEnt_Type(mmap);

        // handle entries of the allocatable memory type
        if(type == MMAP_FREE) {
            // ignore regions below the 1M mark
            if(MMapEnt_Ptr(mmap) < 0x100000) {
                if(gLogPhysRegions) log("Ignoring conventional memory at $%016lx (size %016lx)",
                        MMapEnt_Ptr(mmap), MMapEnt_Size(mmap));
                continue;
            }

            // store it
            gPhysRegions[gNumPhysRegions].start = MMapEnt_Ptr(mmap);
            gPhysRegions[gNumPhysRegions].length = MMapEnt_Size(mmap);

            if(gLogPhysRegions) log("phys region %2u: start $%016lx len %016lx", gNumPhysRegions,
                    gPhysRegions[gNumPhysRegions].start, gPhysRegions[gNumPhysRegions].length);

            gNumPhysRegions++;
        } else {
            if(gLogPhysRegions) log("Unused Entry: addr $%016lx size %016lx flags %08x",
                    MMapEnt_Ptr(mmap), MMapEnt_Size(mmap), MMapEnt_Type(mmap));
        }
    }


    // create holes for the kernel
    create_kernel_hole();
}

/**
 * Creates a hole for the kernel text and data/bss sections, so that we'll never try to allocate
 * over them.
 *
 * We don't actually have to do anything here (I think) since it seems that BOOTBOOT excludes the
 * area where the kernel was loaded from the memory map it returns to us. However, the bootboot
 * info and environment structures may not have this guarantee.
 */
static void create_kernel_hole() {
}



/**
 * Return the number of physical memory maps.
 */
int platform_phys_num_regions() {
    // if no regions have been loaded, abort out
    if(!gNumPhysRegions) {
        return -1;
    }

    return gNumPhysRegions;
}


/**
 * Gets info out of the nth physical allocation region
 */
int platform_phys_get_info(const size_t idx, uint64_t *addr, uint64_t *length) {
    // ensure in bounds
    if(idx >= gNumPhysRegions) {
        return -1;
    }

    // copy out address and length
    *addr = gPhysRegions[idx].start;
    *length = gPhysRegions[idx].length;

    return 0;
}




/**
 * Returns the information on kernel sections.
 */
int platform_section_get_info(const platform_section_t section, uint64_t *physAddr,
        uintptr_t *virtAddr, uintptr_t *length) {
    // TODO: get the phys addresses

    switch(section) {
        // kernel text segment
        case kSectionKernelText:
            *virtAddr = (uintptr_t) (&__kern_code_start);
            *length = ((uintptr_t) &__kern_code_end) - ((uintptr_t) &__kern_code_start);
            *physAddr = Physmap::gKernelTextPhys;
            return 0;

        // kernel data segment
        case kSectionKernelData:
            *virtAddr = (uintptr_t) &__kern_data_start;
            *length = (uintptr_t) &__kern_data_size;
            *physAddr = Physmap::gKernelDataPhys;
            return 0;

        // kernel BSS segment
        case kSectionKernelBss:
            *virtAddr = (uintptr_t) &__kern_bss_start;
            *length = (uintptr_t) &__kern_bss_size;
            *physAddr = Physmap::gKernelBssPhys;
            return 0;

        // unsupported section
        default:
            return -1;
    }
}


/**
 * Reserves memory for a module.
 */
void physmap_module_reserve(const uintptr_t start, const uintptr_t end) {
    if(end > gModulesEnd) {
        // round up to nearest page size
        const auto pageSz = arch_page_size();
        const size_t roundedUpAddr = ((end + pageSz - 1) / pageSz) * pageSz;

        gModulesEnd = roundedUpAddr;
    }
}

/**
 * Determines the base address of the kernel's .text, .data, and .bss segments in physical memory
 * space by resolving the virtual addresses in the current memory mamp.
 *
 * This assumes that the segments are loaded in contiguous physical memory pages.
 */
void Physmap::DetectKernelPhys() {
    using namespace arch::vm;
    int err;

    // figure out what page table the bootloader gave us
    uintptr_t pml4 = 0;
    asm volatile("mov %%cr3, %0" : "=r"(pml4));

    // determine text base
    const uintptr_t textBase = reinterpret_cast<uintptr_t>(&__kern_code_start);
    err = ResolvePml4Virt(pml4, textBase, gKernelTextPhys);
    REQUIRE(!err, "failed to resolve kernel %s base (%p): %u", ".text", textBase, err);

    // determine data base
    const uintptr_t dataBase = reinterpret_cast<uintptr_t>(&__kern_data_start);
    err = ResolvePml4Virt(pml4, dataBase, gKernelDataPhys);
    REQUIRE(!err, "failed to resolve kernel %s base (%p): %u", ".data", dataBase, err);

    // determine bss base
    const uintptr_t bssBase = reinterpret_cast<uintptr_t>(&__kern_bss_start);
    err = ResolvePml4Virt(pml4, bssBase, gKernelBssPhys);
    REQUIRE(!err, "failed to resolve kernel %s base (%p): %u", ".bss", bssBase, err);

    // get bootboot phys address: it's always required
    const uintptr_t bootBase = reinterpret_cast<uintptr_t>(&bootboot);
    err = ResolvePml4Virt(pml4, bootBase, gBootInfoPhys);
    REQUIRE(!err, "failed to resolve kernel %s base (%p): %u", "bootboot", bootBase, err);

    // then, check the environment block
    const uintptr_t envBase = reinterpret_cast<uintptr_t>(&environment);
    err = ResolvePml4Virt(pml4, envBase, gBootEnvPhys);
    REQUIRE(!err, "failed to resolve kernel %s base (%p): %u", "environment", envBase, err);

    if(gLogPhysRegions) log("Phys base: .text $%p .data $%p .bss $%p; info $%p env $%p",
            gKernelTextPhys, gKernelDataPhys, gKernelBssPhys, gBootInfoPhys, gBootEnvPhys);
}

/**
 * Perform some platform-specific updates to the kernel VM map.
 *
 * In our case, this ensures the bootboot info and environment blocks are mapped at the correct
 * addresses. This is really just necessary for the root server but it's probably good to just do
 * this (the memory isn't going away) so we can continue to refer back to it.
 */
void platform::KernelMapEarlyInit() {
    int err;

    auto vm = vm::Map::kern();
    REQUIRE(vm, "failed to get kernel vm map");

    const auto pageSz = arch_page_size();

    // map the bootboot structure
    const uintptr_t bootBase = reinterpret_cast<uintptr_t>(&bootboot);
    err = vm->add(Physmap::gBootInfoPhys, pageSz, bootBase, vm::MapMode::kKernelRead);
    REQUIRE(!err, "failed to map %s: %u", "bootboot info", err);

    // map the environment structure
    const uintptr_t envBase = reinterpret_cast<uintptr_t>(&environment);
    err = vm->add(Physmap::gBootEnvPhys, pageSz, envBase, vm::MapMode::kKernelRead);
    REQUIRE(!err, "failed to map %s: %u", "boot environment", err);

}
