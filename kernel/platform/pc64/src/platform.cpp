#include <platform.h>
#include <log.h>

#include "physmap.h"
#include "acpi/Parser.h"
#include "io/spew.h"
#include "irq/Manager.h"
#include "irq/pic.h"
#include "timer/pit.h"

#include <bootboot.h>
#include <stdint.h>

using namespace platform;

/// bootloader info
extern "C" BOOTBOOT bootboot;

/**
 * Initializes the platform code.
 */
void platform_init() {
    // configure debug printing
    Spew::Init();

    // parse phys mapping
    Physmap::Init();
    Physmap::DetectKernelPhys();

    // disable legacy stuff
    irq::LegacyPic::disable();
    timer::LegacyPIT::disable();

    // set up interrupt manager
    IrqManager::Init();
}

/**
 * Once VM is available, perform some initialization. We'll parse some basic ACPI tables in order
 * to set up interrupts.
 */
void platform_vm_available() {
    AcpiParser::Init();
}
