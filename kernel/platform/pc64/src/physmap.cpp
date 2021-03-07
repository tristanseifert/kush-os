#include "physmap.h"

#include <bootboot.h>

#include <log.h>
#include <platform.h>

#include <arch.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern size_t __kern_keep_start;
extern size_t __kern_code_start, __kern_code_end;
extern size_t __kern_data_start, __kern_data_size;
extern size_t __kern_bss_start, __kern_bss_size;
extern size_t __kern_keep_start, __kern_keep_end;

/// Maximum number of physical memory regions to allocate space for
#define MAX_REGIONS             10

/// Minimum length of allocatable regions (anything smaller is ignored)
constexpr static const size_t kRegionSizeThreshold = (512 * 1024);

/// Total number of allocatable physical RAM regions
static size_t gNumPhysRegions = 0;
/// Info on each of the physical regions
static physmap_region_t gPhysRegions[MAX_REGIONS] = {
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
void physmap_parse_bootboot(const BOOTBOOT *boot) {
    // prepare our buffer
    gNumPhysRegions = 0;
    memclr(&gPhysRegions, sizeof(physmap_region_t) * MAX_REGIONS);

    // iterate over all entries
    const MMapEnt *mmap = nullptr;
    for(mmap = &boot->mmap; (const uint8_t *) mmap < (const uint8_t *) boot + boot->size; mmap++) {
        const auto type = MMapEnt_Type(mmap);

        // handle entries of the allocatable memory type
        if(type == MMAP_FREE) {
            // ignore regions below the 1M mark
            if(MMapEnt_Ptr(mmap) < 0x100000) {
                log("Ignoring conventional memory at %016llx (size %016llx)", MMapEnt_Ptr(mmap),
                        MMapEnt_Size(mmap));
                continue;
            }

            // ignore if the region is too small (< 512K)
            if(MMapEnt_Size(mmap) < kRegionSizeThreshold) {
                log("Ignoring memory at %016x (size %u bytes): too small", MMapEnt_Ptr(mmap),
                        MMapEnt_Size(mmap));
                continue;
            }

            // store it
            gPhysRegions[gNumPhysRegions].start = MMapEnt_Ptr(mmap);
            gPhysRegions[gNumPhysRegions].length = MMapEnt_Size(mmap);

            if(gLogPhysRegions) log("phys region %2u: start %016x len %016x", gNumPhysRegions,
                    gPhysRegions[gNumPhysRegions].start, gPhysRegions[gNumPhysRegions].length);

            gNumPhysRegions++;
        } else {
            /*log("Unused Entry: addr %016llx size %016llx flags %08x", MMapEnt_Ptr(mmap),
                    MMapEnt_Size(mmap), MMapEnt_Type(mmap));*/
        }
    }


    // create holes for the kernel
    create_kernel_hole();
}

/**
 * Creates a hole for the kernel text and data/bss sections, so that we'll never try to allocate
 * over them.
 */
static void create_kernel_hole() {
    // TODO: implement
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
            return 0;

        // kernel data segment
        case kSectionKernelData:
            *virtAddr = (uintptr_t) &__kern_data_start;
            *length = (uintptr_t) &__kern_data_size;
            return 0;

        // kernel BSS segment
        case kSectionKernelBss:
            *virtAddr = (uintptr_t) &__kern_bss_start;
            *length = (uintptr_t) &__kern_bss_size;
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

