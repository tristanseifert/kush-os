#include "physmap.h"
#include "multiboot.h"

#include <log.h>
#include <platform.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern size_t __kern_code_start, __kern_code_end;
extern size_t __kern_keep_start, __kern_keep_end;

/// Maximum number of physical memory regions to allocate space for
#define MAX_REGIONS             10

/// Total number of allocatable physical RAM regions
static size_t gNumPhysRegions = 0;
/// Info on each of the physical regions
static physmap_region_t gPhysRegions[MAX_REGIONS];

static void create_kernel_hole(multiboot_info_t *info);



/**
 * Parse the multiboot structure for all of the memory in the machine.
 */
void physmap_load_from_multiboot(void *_info) {
    // validate the info struct
    if(!_info) {
        panic("failed to get multiboot info");
    }

    // TODO: it's possible this is outside the first 4M of RAM
    multiboot_info_t *info = (multiboot_info_t *) _info;

    if(!(info->flags & MULTIBOOT_INFO_MEM_MAP)) {
        panic("multiboot info is missing %s", "memory map");
    }

    // parse memory map
    const multiboot_memory_map_t *entries = (const multiboot_memory_map_t *) info->mmap_addr;
    const uint32_t numEntries = info->mmap_length / sizeof(multiboot_memory_map_t);

    gNumPhysRegions = 0;
    memclr(&gPhysRegions, sizeof(physmap_region_t) * MAX_REGIONS);

    for(size_t i = 0; i < numEntries; i++) {
        // ensure the entry is the proper size
        const multiboot_memory_map_t *e = &entries[i]; 
        if(e->size != sizeof(multiboot_memory_map_t) - offsetof(multiboot_memory_map_t, addr)) {
            // XXX: are longer stride values allowed?
            panic("invalid multiboot memory map entry size: %ld", e->size);
        }

        // add entries for allocatable memory
        if(e->type == 1) {
            // ignore regions below the 1M mark
            if(e->addr < 0x100000) {
                log("Ignoring conventional memory at %016llx (size %016llx)", e->addr, e->len);
                continue;
            }

            gPhysRegions[gNumPhysRegions].start = e->addr;
            gPhysRegions[gNumPhysRegions].length = e->len;
            gNumPhysRegions++;
        } else {
            //log("Unused Entry %02lu: addr %016llx size %016llx flags %08lx", i, e->addr, e->len, e->type);
        }
    }

    // create holes for the kernel
    create_kernel_hole(info);
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
static void create_kernel_hole(multiboot_info_t *info) {
    // calculate physical range of kernel text
    const size_t physStart = ((size_t) &__kern_keep_start) - 0xC0000000;
    const size_t physEnd = ((size_t) &__kern_keep_end) - 0xC0000000;

    log("Kernel memory physical range: $%08lx to $%08lx", physStart, physEnd);

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

