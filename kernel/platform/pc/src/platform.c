/**
 * Initializers and other setup/teardown for the x86 IBM PC-type platform.
 */
#include <platform.h>
#include <log.h>

#include "multiboot.h"
#include "irq/pic.h"
#include "io/spew.h"

#include <stdint.h>


/**
 * Initializes the platform code.
 *
 * This sets up the PC-specific interrupt handling stuff.
 */
void platform_init() {
    // configure debug printing
    serial_spew_init();

    // parse multiboot info
    multiboot_parse();

    // set up and remap the PICs and other interrupt controllers
    pic_init();

    // install our IRQ handlers
}
