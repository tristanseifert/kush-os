/**
 * Initializers and other setup/teardown for the x86 IBM PC-type platform.
 */
#include <platform.h>
#include <log.h>

#include "multiboot.h"
#include "irq/pic.h"
#include "irq/Manager.h"
#include "timer/Manager.h"
#include "timer/pit.h"
#include "io/spew.h"
#include "acpi/Manager.h"

#include <stdint.h>

using namespace platform;

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
    *((volatile uint16_t *) 0xb800C) = 0x4146;

    // set up and remap the PICs and other interrupt controllers
    pic_init();
    timer::LegacyPIT::disable();

    *((volatile uint16_t *) 0xb800E) = 0x4147;
}

/**
 * Once VM is available, perform some initialization. We'll parse some basic ACPI tables in order
 * to set up interrupts.
 */
void platform_vm_available() {

    irq::Manager::init();
    acpi::Manager::vmAvailable();
    timer::Manager::init();
    irq::Manager::setupIrqs();
}
