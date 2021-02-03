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
 * Notifies platform code that virtual memory is available.
 */
void platform_vm_available();

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



/**
 * Regions of the kernel image that the kernel is interested in. This is used to build the virtual
 * memory maps for the kernel code and data.
 */
typedef enum platform_section {
    kSectionKernelText                  = 1,
    kSectionKernelData                  = 2,
    kSectionKernelBss                   = 3,
    kSectionKernelStack                 = 4,
} platform_section_t;

/**
 * Gets information on the given section, if available.
 *
 * @param physAddr Physical (load) address of the relevant section, or 0 if unavailable
 * @param virtAddr Virtual address of the relevant section
 * @param length Length of the section, in bytes.
 *
 * @return 0 on success.
 */
int platform_section_get_info(const platform_section_t section, uint64_t *physAddr,
        uintptr_t *virtAddr, uintptr_t *length);

#ifdef __cplusplus
}
#endif

#endif
