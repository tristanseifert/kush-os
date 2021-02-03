#ifndef PLATFORM_PC_PHYSMAP_H
#define PLATFORM_PC_PHYSMAP_H

#include <stdint.h>

struct multiboot_tag_mmap;

/**
 * Start and length of memory regions
 */
typedef struct physmap_region {
    /// Starting physical address of the memory region
    uint64_t start;
    /// Size of the memory region, in length
    uint64_t length;
} physmap_region_t;

/**
 * Initializes the platform memory map with the given multiboot structure.
 */
void physmap_load_from_multiboot(struct multiboot_tag_mmap *info);

#endif
