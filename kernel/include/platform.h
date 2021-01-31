#ifndef KERN_PLATFORM_H
#define KERN_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Performs platform-specific initialization.
 */
void platform_init();

/**
 * Writes a character to the platform debug spew, if any.
 */
void platform_debug_spew(char ch);

/**
 * Panic callback; this should disable interrupts, then halt the machine.
 */
void platform_panic_handler() __attribute__((noreturn));



/**
 * Returns the number of physical memory regions that maybe used for memory allocation.
 *
 * @return Number of memory regions, or a negative reror code
 */
int platform_phys_num_regions();

/**
 * Gets information on a physical memory region.
 */
int platform_phys_get_info(const size_t idx, uint64_t *addr, uint64_t *length);

#ifdef __cplusplus
}
#endif

#endif
