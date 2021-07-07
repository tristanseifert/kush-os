#include <platform.h>
#include <log.h>

#include "physmap.h"
#include "acpi/Parser.h"
#include "io/spew.h"
#include "irq/Manager.h"
#include "timer/Hpet.h"
#include "timer/pit.h"
#include "timer/Tsc.h"

#include <version.h>
#include <printf.h>
#include <debug/FramebufferConsole.h>
#include <sched/Task.h>
#include <mem/PhysicalAllocator.h>
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
static void PrintBootMsg();

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
    PrintBootMsg();

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
 * Generates entropy via the RDRAND instruction. We've guaranteed that it's available via the
 * amd64 architecture setup code (it checks CPUID for this)
 */
int platform::GetEntropy(void *out, const size_t outBytes) {
    const auto blocks = (outBytes + 3) / 4;
    auto writePtr = reinterpret_cast<uint8_t *>(out);
    auto writeBytes = outBytes;

    for(size_t i = 0; i < blocks; i++) {
        uint32_t temp, status;
        asm volatile("rdrand %0" : "=r"(temp), "=@ccc"(status));
        if(!status) {
            return -1;
        }

        const auto nb = (writeBytes > sizeof(temp)) ? sizeof(temp) : writeBytes;
        memcpy(writePtr, &temp, nb);
        writePtr += nb;
    }

    return 0;
}



/**
 * Sets the framebuffer console state. If it's currently enabled, we'll deallocate it and remove
 * the VM allocation.
 */
int platform::SetConsoleState(const bool enabled) {
    // bail if the state is already achieved
    if(enabled && gConsole) return 0;
    else if(!enabled && !gConsole) return 0;

    // need to enable
    if(enabled) {
        InitFbCons();
    }
    // need to disable
    else {
        delete gConsole;
        gConsole = nullptr;
    }

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

/**
 * Prints some information about the system to the framebuffer console.
 */
static void PrintBootMsg() {
    int nChars;
    char buf[100]{0};

    constexpr static const size_t kInfoBoxWidth{50};

    // version
    char hash[9]{0};
    memcpy(hash, gVERSION_HASH, 8);
    nChars = snprintf(buf, sizeof(buf), "kush-os (%s, built on %s) - "
        "Copyright 2021 Tristan Seifert\n\n", hash, __DATE__);
    gConsole->write(buf, nChars);

    // top info box
    gConsole->write(" \x05", 2);
    for(size_t i = 0; i < kInfoBoxWidth; i++) {
        gConsole->write(0x01);
    }
    gConsole->write("\x06\n", 2);

    // CPUs (TODO: use the actual number of started cores)
    gConsole->write(" \x00", 2);
    nChars = snprintf(buf, sizeof(buf), "%32s: %lu", "Processors Available", bootboot.numcores);
    gConsole->write(buf, nChars);

    for(size_t i = nChars; i < kInfoBoxWidth; i++) {
        gConsole->write(' ');
    }
    gConsole->write("\x00\n", 2);

    // available memory
    gConsole->write(" \x00", 2);

    nChars = snprintf(buf, sizeof(buf), "%32s: %lu K", "Available physical memory",
            mem::PhysicalAllocator::getTotalPages() * (arch_page_size() / 1024ULL));
    gConsole->write(buf, nChars);

    for(size_t i = nChars; i < kInfoBoxWidth; i++) {
        gConsole->write(' ');
    }
    gConsole->write("\x00\n", 2);

    // bottom info box
    gConsole->write(" \x04", 2);
    for(size_t i = 0; i < kInfoBoxWidth; i++) {
        gConsole->write(0x01);
    }
    gConsole->write("\x03\n", 2);

    // and the start message
    gConsole->write("\n\e[32mSystem is starting up...\e[0m");
}
