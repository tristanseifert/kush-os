/**
 * Initializers and other setup/teardown for the x86 IBM PC-type platform.
 */
#include <platform.h>
#include <log.h>

#include "physmap.h"
#include "irq/pic.h"
#include "io/spew.h"

#include <stdint.h>

/// physical address of the multiboot structure
uint32_t x86_multiboot_info = 0;

/**
 * Initializes the platform code.
 *
 * This sets up the PC-specific interrupt handling stuff.
 */
void platform_init() {
    // configure debug printing
    serial_spew_init();

    // parse the memory regions from multiboot struct
    physmap_load_from_multiboot((void *) x86_multiboot_info);


    // set up and remap the PICs and other interrupt controllers
    pic_init();

    // install our IRQ handlers
}
