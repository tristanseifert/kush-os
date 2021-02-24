#include "physmap.h"
#include "multiboot2.h"

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

/// Total number of allocatable physical RAM regions
static size_t gNumPhysRegions = 0;
/// Info on each of the physical regions
static physmap_region_t gPhysRegions[MAX_REGIONS] = {
    {0, 0},
};

/// end address of the modules region
static uintptr_t gModulesEnd = 0;

static void create_kernel_hole();



/**
 * Parse the multiboot structure for all of the memory in the machine.
 */
void physmap_load_from_multiboot(struct multiboot_tag_mmap *tag) {
    multiboot_memory_map_t *mmap = tag->entries;

    // prepare our buffer
    gNumPhysRegions = 0;
    memclr(&gPhysRegions, sizeof(physmap_region_t) * MAX_REGIONS);

    // iterate over all entries
    for(mmap = tag->entries; (uint8_t *) mmap < (uint8_t *) tag + tag->size;
        mmap = (multiboot_memory_map_t *) ((uintptr_t) mmap + tag->entry_size)) {
        // handle entries of the allocatable memory type
        if(mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            // ignore regions below the 1M mark
            if(mmap->addr < 0x100000) {
                log("Ignoring conventional memory at %016llx (size %016llx)", mmap->addr, mmap->len);
                continue;
            }

            gPhysRegions[gNumPhysRegions].start = mmap->addr;
            gPhysRegions[gNumPhysRegions].length = mmap->len;
            gNumPhysRegions++;
        } else {
            // log("Unused Entry: addr %016llx size %016llx flags %08x", mmap->addr, mmap->len, mmap->type);
        }
    }


    // create holes for the kernel
    create_kernel_hole();
}

/**
 * Creates a hole for the kernel text and data/bss sections.
 *
 * We know that the bootloader loads us to 0x100000; taking into account the fact that all the
 * addresses in the ELF header are offset by 0xC0000000 (because that's where we're MAPPED at) we
 * can figure out what area of physical memory we need to carve out.
 *
 * The linker defines for us some handy variables that indicate exactly the chunk of memory that
 * we need to keep.
 *
 * Note that this obviously blows up pretty spectacularly if the initial assumption (re: load
 * address) is violated. qemu seems to obey it, though.
 */
static void create_kernel_hole() {
    // calculate physical range of kernel text
    const size_t physStart = ((size_t) &__kern_keep_start);// - 0xC0000000;
    size_t physEnd = ((size_t) &__kern_keep_end) - 0xC0000000;

    if(gModulesEnd > physEnd) {
        log("wasting %d bytes for modules!", (gModulesEnd - physEnd));
        physEnd = gModulesEnd;
    }

    log("Kernel memory physical range: $%08x to $%08x", physStart, physEnd);

    // first case; a physical map starts at where we're loaded
    for(size_t i = 0; i < gNumPhysRegions; i++) {
        if(gPhysRegions[i].start == physStart) {
            gPhysRegions[i].start += (physEnd - physStart);
            gPhysRegions[i].length -= (physEnd - physStart);
        }
    }

    // TODO: handle the case where we need to truncate an existing region
    // TODO: handle the case where we need to split an existing region
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
 *
 * We take the knowledge that the virtual address of all sections is its load address plus the
 * constant 0xC0000000.
 */
int platform_section_get_info(const platform_section_t section, uint64_t *physAddr,
        uintptr_t *virtAddr, uintptr_t *length) {
    switch(section) {
        // kernel text segment
        case kSectionKernelText:
            *virtAddr = (uintptr_t) (&__kern_code_start) - 0x4000;
            *physAddr = (*virtAddr) - 0xC0000000;
            *length = ((uintptr_t) &__kern_code_end) - ((uintptr_t) &__kern_code_start) + 0x4000;
            return 0;

        // kernel data segment
        case kSectionKernelData:
            *virtAddr = (uintptr_t) &__kern_data_start;
            *physAddr = (*virtAddr) - 0xC0000000;
            *length = (uintptr_t) &__kern_data_size;
            return 0;

        // kernel BSS segment
        case kSectionKernelBss:
            *virtAddr = (uintptr_t) &__kern_bss_start;
            *physAddr = (*virtAddr) - 0xC0000000;
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

