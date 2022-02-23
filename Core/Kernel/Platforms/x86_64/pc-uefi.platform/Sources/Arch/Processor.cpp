#include "Processor.h"
#include "ExceptionHandlers.h"
#include "Util/Backtrace.h"

#include <Logging/Console.h>
#include <Runtime/Printf.h>

#include <cpuid.h>

using namespace Platform::Amd64Uefi;

/// EFER flag for SYSCALL/SYSRET
#define X86_MSR_EFER_SCE                (1 << 0)
/// EFER flag for NX bit
#define X86_MSR_EFER_NX                 (1 << 11)

/**
 * Halts all processors in the system.
 *
 * This is accomplished by sending an IPI to each processor, waiting until they all reply (or a
 * timeout expires) and then halting ourselves.
 */
[[noreturn]] void Processor::HaltAll() {
    // TODO: IPI stuff

    // we're done, halt ourselves
    HaltSelf();
}

/**
 * Formats the processors' register state to the string buffer provided.
 *
 * @param state Registers to read
 * @param out Output string buffer
 * @param outSize Number of characters of space in the buffer
 *
 * @return Number of characters written
 */
int Processor::Regs::Format(const Regs &state, char *out, const size_t outSize) {
    const char *errorName{nullptr};

    // is it an exception?
    if(state.irq & 0x1000) {
        errorName = ExceptionHandlers::GetExceptionName(state.irq, true);
    }

    return snprintf_(out, outSize,
            "RAX %016llx RBX %016llx\n"
            "RCX %016llx RDX %016llx\n"
            "RSI %016llx RDI %016llx\n"
            "RBP %016llx  R8 %016llx\n"
            " R9 %016llx R10 %016llx\n"
            "R11 %016llx R12 %016llx\n"
            "R13 %016llx R14 %016llx\n"
            "R15 %016llx\n"
            "RFLAGS %016llx Error %llx:%llx (%s)\n"
            "RIP %04x:%016llx RSP %04x:%16llx",
            state.rax, state.rbx, state.rcx, state.rdx, state.rsi, state.rdi, state.rbp, state.r8,
            state.r9, state.r10, state.r11, state.r12, state.r13, state.r14, state.r15,
            state.rflags, state.irq, state.errorCode, errorName ? errorName : "unknown",
            state.cs, state.rip, state.ss, state.rsp);
}

/**
 * Attempt to output a backtrace for the stack referred to by this register state.
 *
 * This only works for stacks in kernel space.
 *
 * @param state Register state to read
 * @param out Output string buffer
 * @param outSize Number of characters of space in the buffer
 *
 * @return Number of stack frames output, or -1 if we couldn't format the stack.
 */
int Processor::Regs::Backtrace(const Regs &state, char *out, const size_t outSize) {
    // validate RBP
    if(state.rbp < 0x8000'0000'0000'0000ULL) return -1;

    // produce backtrace
    return Backtrace::Print(reinterpret_cast<const void *>(state.rbp), out, outSize, true);
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
} kRequiredCpuFeatures[] = {
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
    // XSAVE support
    {
        .leaf   = 0x01,
        .ecx    = (1 << 26),
        .name   = "XSAVE",
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
 * Ensure all required CPU features are supported.
 *
 * If the processor does not support a mandatory feature, the machine is halted.
 */
void Processor::VerifyFeatures() {
    uint32_t eax, ebx, ecx, edx;

    // test each feature
    auto feature = &kRequiredCpuFeatures[0];
    while(feature->name) {
        // perform CPUID
        if(!__get_cpuid(feature->leaf, &eax, &ebx, &ecx, &edx)) {
            PANIC("cpuid leaf $%08x not supported", feature->leaf);
        }

        if((eax & feature->eax) != feature->eax || (ebx & feature->ebx) != feature->ebx ||
            (ecx & feature->ecx) != feature->ecx || (edx & feature->edx) != feature->edx) {
            PANIC("CPU does not support '%s'! (%08x %08x %08x %08x, masks %08x %08x %08x %08x)",
                    feature->name, eax, ebx, ecx, edx, feature->eax, feature->ebx, feature->ecx,
                    feature->edx);
        }

        // go to next feature
        feature++;
    }
}

/**
 * Initializes and enables processor features.
 */
void Processor::InitFeatures() {
    uint32_t lo, hi;

    // enable SYSCALL
    ReadMsr(Msr::EFER, lo, hi);

    lo |= X86_MSR_EFER_SCE;

    WriteMsr(Msr::EFER, lo, hi);
}
