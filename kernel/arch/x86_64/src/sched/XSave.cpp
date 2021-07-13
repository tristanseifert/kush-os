#include "XSave.h"
#include "ThreadState.h"

#include <arch/x86_msr.h>
#include <cpuid.h>
#include <stdint.h>

#include <log.h>
#include <mem/Heap.h>

using namespace arch;

/// Toggle debug logging of XSAVE support
constexpr static const bool kLogging{true};

/// Alignment of the XSAVE area, in bytes
constexpr static const size_t kXSaveRegionAlignment{64};
/// Size of an XSAVE area, in bytes
static size_t gXSaveAreaSize{0};

/// Bitmap of supported XSAVE state
static uint64_t gXSaveStateSupported{0};
/// Currently allocated bits in XCR0 that correspond to XSAVE state
constexpr static const uint64_t kXcr0Mask{0b10011101111111111};

/// Is XSAVEOPT supported?
static bool gSupportsXSaveOpt{false};
/// Are compaction extensions supported?
static bool gSupportsCompaction{false};
/// Is XSAVES (supervisor extensions) supported?
static bool gSupportsXSaveSup{false};

static void PatchContextSwitch();



/**
 * Initializes support for the XSave instructions that are used to save and restore the state of
 * floating point and vector registers. This roughly follows the following:
 *
 * - Enable XSAVE support in CR4.
 * - Read CPUID to determine available state that can be managed with XSAVE; this also gets the
 *   size, in bytes, for a full XSAVE area.
 * - Read CPUID to determine support for XSAVEOPT and compaction extensions.
 * - Enable XSAVE to manage all available processor state
 * - If needed, patch the context switching routines to use the correct version of the state save
 *   and restore instructions.
 *
 * When this is executed, we know at least the basic XSAVE instruction set is availble, as this is
 * tested for by the early boot CPU verification code.
 */
void arch::InitXSave() {
    int err;
    uint32_t eax, ebx, ecx, edx;
    uint64_t temp;

    // enable XSAVE
    asm volatile("movq %%cr4,%0" : "=r"(temp) ::);
    temp |= (1 << 18);
    asm volatile("movq %0, %%cr4" :: "r"(temp) :);

    // ensure that CPUID leaf 0x0D is supported
    err = __get_cpuid_max(0, nullptr);
    REQUIRE(err >= 0xD, "CPUID max too low: got $%x, expected at least $D", err);

    /*
     * Read out the XSAVE information via CPUID leaf 0x0D, sub function 0x00. Since we need to pass
     * in the function (in %ecx) we can't use the CPUID builtin and have to use ~ inline assembly ~
     * to make this work.
     */
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(0x0D), "2"(0x0));

    gXSaveAreaSize = ecx;
    gXSaveStateSupported = static_cast<uint64_t>(eax) | (static_cast<uint64_t>(edx) << 32);

    if(kLogging) log("XSave state supported: $%016lx, region is %lu bytes", gXSaveStateSupported,
            gXSaveAreaSize);

    /**
     * Check to see if we support XSAVEOPT, compaction extensions, and XSAVES.
     */
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(0x0D), "2"(0x1));

    gSupportsXSaveOpt = eax & (1 << 0);
    gSupportsCompaction = eax & (1 << 1);
    gSupportsXSaveSup = eax & (1 << 3);

    /*
     * Allow XSAVE to manage all state supported on this processor.
     */
    asm volatile("xgetbv" : "=d"(edx), "=a"(eax) : "c"(0));
    temp = (static_cast<uint64_t>(edx) << 32) | static_cast<uint64_t>(eax);
    temp &= ~kXcr0Mask;
    temp |= gXSaveStateSupported & kXcr0Mask;
    asm volatile("xsetbv" :: "a"(temp & 0xFFFFFFFF), "d"(temp >> 32), "c"(0));


    // patch context switch code
    PatchContextSwitch();
}

/**
 * Patches the XSAVE/XRSTOR instructions in the context switching code, if needed. This is required
 * to support optional features like XSAVEOPT without checking on every context switch.
 *
 * This works as the architecture init code runs before virtual memory is set up, at which point
 * we're still running with the BOOTBOOT loader's initial mapping that has the entirety of the
 * kernel mapped as RWX. This is shit for security but lets us bring back the wonderful tricks of
 * 1980s assembly programming with self modifying code :D
 */
static void PatchContextSwitch() {

}



/**
 * Allocates the XSAVE region for the given thread's state.
 */
void arch::AllocXSaveRegion(ThreadState &ts) {
    REQUIRE(!ts.fpuState, "cannot realloc XSAVE region");

    // just allocate a region large enough with a suitable alignment
    ts.fpuState = mem::Heap::allocAligned(gXSaveAreaSize, kXSaveRegionAlignment);
    memset(ts.fpuState, 0, gXSaveAreaSize);
}

/**
 * Returns the size of the XSave area; this is determined during the XSave init phase, so this
 * shall not be called during very early boot. It should only be used by context switching and
 * scheduler code anyhow.
 */
size_t arch::GetXSaveRegionSize() {
    return gXSaveAreaSize;
}

