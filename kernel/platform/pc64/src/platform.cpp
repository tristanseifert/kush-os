#include <platform.h>
#include <log.h>

#include "physmap.h"
#include "acpi/Parser.h"
#include "io/spew.h"
#include "irq/Manager.h"
#include "timer/Hpet.h"
#include "timer/pit.h"
#include "timer/Tsc.h"

#include <bootboot.h>
#include <stdint.h>
#include <x86intrin.h>

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
    LegacyPIT::disable();

    // set up interrupt manager
    IrqManager::Init();
}

/**
 * Once VM is available, perform some initialization. We'll parse some basic ACPI tables in order
 * to set up interrupts.
 */
void platform_vm_available() {
    AcpiParser::Init();

    // set up the system timer and BSP TSC
    Hpet::Init();
    Tsc::InitCoreLocal();

    // then, set up the interrupt controllers (both system and BSP local)
    IrqManager::InitSystemControllers();
    IrqManager::InitCoreLocalController();

    // prepare stacks, per core info, etc. for all APs

    // signal APs to start and wait
}

/**
 * Read out the TSC of the processor for the core local timestamp.
 */
uint64_t platform_local_timer_now() {
    return __rdtsc();
}

