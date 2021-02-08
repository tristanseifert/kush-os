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

    // set up and remap the PICs and other interrupt controllers
    pic_init();
    timer::LegacyPIT::disable();
}

/**
 * Once VM is available, perform some initialization. We'll parse some basic ACPI tables in order
 * to set up interrupts.
 */
void platform_vm_available() {
    irq::Manager::init();
    acpi::Manager::vmAvailable();
    irq::Manager::setupIrqs();
    timer::Manager::init();
}
