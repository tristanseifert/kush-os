#include <platform.h>
#include <log.h>

#include "physmap.h"
#include "acpi/Parser.h"
#include "io/spew.h"
#include "irq/Manager.h"
#include "timer/Hpet.h"
#include "timer/pit.h"
#include "timer/Tsc.h"

#include <debug/FramebufferConsole.h>
#include <sched/Task.h>
#include <vm/Map.h>
#include <vm/MapEntry.h>

#include <arch.h>
#include <bootboot.h>
#include <stdint.h>
#include <x86intrin.h>

namespace platform {
    /// shared framebuffer console
    debug::FramebufferConsole *gConsole = nullptr;
};
static void InitFbCons();

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
void platform::VmAvailable() {
    AcpiParser::Init();

    // set up the system timer and BSP TSC
    Hpet::Init();
    Tsc::InitCoreLocal();

    // then, set up the interrupt controllers (both system and BSP local)
    IrqManager::InitSystemControllers();
    IrqManager::InitCoreLocalController();

    // set up the framebuffer console
    InitFbCons();

    // prepare stacks, per core info, etc. for all APs

    // signal APs to start and wait
}

/**
 * Read out the TSC of the processor for the core local timestamp.
 */
uint64_t platform_local_timer_now() {
    return __rdtsc();
}



/**
 * TODO: implement this
 */
int platform_core_distance(const uintptr_t a, const uintptr_t b) {
    return 0;
}



/**
 * Initialize the global framebuffer console
 */
static void InitFbCons() {
    using namespace debug;
    using namespace vm;
    using ColorOrder = FramebufferConsole::ColorOrder;

    int err;
    uintptr_t fbBase = 0;

    // map the framebuffer into the kernel's VM map
    const auto pageSz = arch_page_size();
    const auto fbMapLen = ((bootboot.fb_size + pageSz - 1) / pageSz) * pageSz;

    auto vm = Map::kern();
    auto entry = MapEntry::makePhys(bootboot.fb_ptr, fbMapLen,
            MappingFlags::RW | MappingFlags::WriteCombine, true);

    err = vm->add(entry, sched::Task::kern(), 0, MappingFlags::None, fbMapLen,
            0xFFFFFF0100000000, 0xFFFFFF01FFFFFFFF);
    REQUIRE(!err, "failed to map framebuffer (phys $%p len %lu): %d", bootboot.fb_ptr, fbMapLen,
            err);

    fbBase = vm->getRegionBase(entry);

    // create console
    ColorOrder fbFormat;

    switch(bootboot.fb_type) {
        case FB_ARGB:
            fbFormat = ColorOrder::ARGB;
            break;
        case FB_RGBA:
            fbFormat = ColorOrder::RGBA;
            break;

        default:
            panic("unsupported framebuffer type %lu", bootboot.fb_type);
    }

    gConsole = new FramebufferConsole(reinterpret_cast<uint32_t *>(fbBase), fbFormat,
            bootboot.fb_width, bootboot.fb_height, bootboot.fb_scanline);
}
