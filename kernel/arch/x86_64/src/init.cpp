#include <arch.h>
#include <stdbool.h>
#include <stdint.h>
#include <printf.h>

#include "IrqRegistry.h"
#include "PerCpuInfo.h"
#include "vm/PTEHandler.h"
#include "syscall/Handler.h"

#include <arch/x86_msr.h>
#include <cpuid.h>

#include "gdt.h"
#include "idt.h"

using namespace arch;

static bool nxEnabled = false;
static void update_supports_nx();
static void TestCpuSupport();

extern arch::vm::PTEHandler *gArchKernelPte;

/// x86_64 stack frame
struct stackframe {
    struct stackframe *rbp;
    uint64_t rip;
};

/**
 * Performs architecture initialization.
 *
 * Specifically, we set up the descriptor tables (GDT) and interrupt table (IDT) for protected
 * mode operation.
 */
void arch_init() {
    // test CPU features we need to support
    TestCpuSupport();

    // determine if we support the NX bit; enable the feature if needed
    update_supports_nx();

    uint32_t lo, hi;
    x86_msr_read(X86_MSR_EFER, &lo, &hi);

    if(nxEnabled) {
        lo |= X86_MSR_EFER_NX;
    }

    lo |= X86_MSR_EFER_SCE; // always enable the SYSCALL bit

    x86_msr_write(X86_MSR_EFER, lo, hi);

    // initialize descriptors
    Gdt::Init();
    Idt::Init();

    // set up some other BSP structures
    IrqRegistry::Init();

    PerCpuInfo::BspInit();
}

/**
 * Initialize some memory pools for allocating paging structures when VM is available.
 */
void arch_vm_available() {
    arch::syscall::Handler::init();
}

/**
 * x86 page size is always 4K. There's also support for 2M and 1G large pages.
 */
size_t arch_page_size() {
    return 4096;
}

/**
 * Whether the processor supports the no-execute bit or not.
 */
bool arch_supports_nx() {
    return nxEnabled;
}



/**
 * Performs a backtrace.
 *
 * The stack parameter should contain the base pointer (%ebp) value, or NULL to start with this
 * function.
 */
int arch_backtrace(void *stack, char *buf, const size_t bufLen) {
    struct stackframe *stk;

    if(!stack) {
        asm volatile("movq %%rbp,%0" : "=r"(stk) ::);
    } else {
        stk = reinterpret_cast<struct stackframe *>(stack);
    }

    // yeet out into the buffer the frames
    char *writePtr = buf;
    int written;

    for(size_t frame = 0; stk && frame < 50; ++frame) {
        const auto bufAvail = bufLen - (writePtr - buf);
        if(!bufAvail) return bufLen;

        written = snprintf(writePtr, bufAvail, "%2zu %016llx\n", frame, stk->rip);

        // prepare for next frame
        writePtr += written;
        stk = stk->rbp;
    }

    return 1;
}




/**
 * Determine if the processor supports no-execute.
 *
 * For x86, we check CPUID leaf $80000001; bit 20 in EDX is set if we support NX, clear if not.
 */
static void update_supports_nx() {
    uint32_t eax, edx, unused1, unused2;
    __get_cpuid(0x80000001, &eax, &unused1, &unused2, &edx);

    nxEnabled = (edx & (1 << 20)) ? true : false;
}



/**
 * Required CPU features. The biggest requirements are SSE 4.1/4.2 and the RDRAND instruction; this
 * means we need an Intel chip that's Ivy Bridge or newer, or an AMD chip released after 2015.
 */
static const struct {
    /// CPUID leaf to query
    uint32_t leaf = 0;
    /// Mask to compare against CPUID results
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    /// Descriptive name of this feature
    const char *name = nullptr;
} kCpuFeatures[] = {
    // APIC support
    {
        .leaf   = 0x01,
        .edx    = (1 << 9),
        .name   = "APIC"
    },
    // POPCNT
    {
        .leaf   = 0x01,
        .ecx    = (1 << 23),
        .name   = "POPCNT"
    },
    // atomic 16-byte compare/exchange
    {
        .leaf   = 0x01,
        .ecx    = (1 << 13),
        .name   = "CMPXCHG16B"
    },
    // SSE 4
    {
        .leaf   = 0x01,
        .ecx    = (1 << 19) | (1 << 20),
        .name   = "SSE 4.1 and SSE 4.2"
    },
   // Hardware RNG
    {
        .leaf   = 0x01,
        .ecx    = (1 << 30),
        .name   = "Hardware RNG (RDRAND)"
    },
    // supervisor mode access protection
    /*{
        .leaf   = 0x07,
        .ebx    = (1 << 20),
        .name   = "SMAP"
    },*/
    // terminator
    {}
};

/**
 * Performs a CPUID request and test whether the features stored in the feature table are all
 * supported.
 */
static void TestCpuSupport() {
    uint32_t eax, ebx, ecx, edx;

    // test each feature
    auto feature = &kCpuFeatures[0];
    while(feature->name) {
        // perform CPUID
        if(!__get_cpuid(feature->leaf, &eax, &ebx, &ecx, &edx)) {
            panic("cpuid leaf $%08x not supported", feature->leaf);
        }

        if((eax & feature->eax) != feature->eax || (ebx & feature->ebx) != feature->ebx ||
            (ecx & feature->ecx) != feature->ecx || (edx & feature->edx) != feature->edx) {
            panic("CPU does not support '%s'! (%08x %08x %08x %08x, masks %08x %08x %08x %08x)",
                    feature->name, eax, ebx, ecx, edx, feature->eax, feature->ebx, feature->ecx,
                    feature->edx);
        }

        // go to next feature
        feature++;
    }
}
