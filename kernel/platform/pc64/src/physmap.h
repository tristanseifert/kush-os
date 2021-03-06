#ifndef PLATFORM_PC64_PHYSMAP_H
#define PLATFORM_PC64_PHYSMAP_H

#include <stdint.h>
#include <bootboot.h>

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
 * Parses a memory map entry provided by the bootloader
 */
void physmap_parse_bootboot(const BOOTBOOT *boot);

/**
 * Reserves memory for a module.
 */
void physmap_module_reserve(const uintptr_t start, const uintptr_t end);

#endif
