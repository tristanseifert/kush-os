#include <platform.h>
#include <log.h>

#include "physmap.h"
#include "io/spew.h"
#include "irq/pic.h"
#include "timer/pit.h"

#include <bootboot.h>
#include <stdint.h>

using namespace platform;

/// bootloader info
extern "C" BOOTBOOT bootboot;

/**
 * Initializes the platform code.
 *
 * At this point, we disable a bunch of legacy stuff that may be enabled.
 */
void platform_init() {
    // configure debug printing
    Spew::Init();

    // parse phys mapping
    log("BootBoot info: %p (protocol %u, %u cores)", &bootboot, bootboot.protocol, bootboot.numcores);
    physmap_parse_bootboot(&bootboot);

    // set up and remap the PICs and other interrupt controllers
    irq::LegacyPic::disable();
    timer::LegacyPIT::disable();
}

/**
 * Once VM is available, perform some initialization. We'll parse some basic ACPI tables in order
 * to set up interrupts.
 */
void platform_vm_available() {

}
