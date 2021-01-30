/**
 * Initializers and other setup/teardown for the x86 IBM PC-type platform.
 */
#include <platform.h>

#include "irq/pic.h"

#include <stdint.h>

/// physical address of the multiboot structure
uint32_t x86_multiboot_info = 0;

/**
 * Initializes the platform code.
 *
 * This sets up the PC-specific interrupt handling stuff.
 */
void platform_init() {
    // set up and remap the PICs and other interrupt controllers
    pic_init();

    // install our IRQ handlers
}
