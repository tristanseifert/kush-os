#ifndef ARCH_X86_THREADSTATE_H
#define ARCH_X86_THREADSTATE_H

// we have to manually define structs offsets for our assembler code
#define TS_OFF_STACKTOP                 0
#define TS_OFF_FPU_ENABLED              8
#define TS_OFF_FXSAVE                   16

#ifndef ASM_FILE
#include <stddef.h>
#include <stdint.h>

namespace arch {
/**
 * Processor state for an x86 thread.
 * 
 * This includes an area for the floating point state. We only save this state when the thread is
 * context switched out if the "FPU used" flag is set. We'll periodically clear this flag if
 * the "FPU count resets" counter is below a certain theshold. So, if a thread uses the floating
 * point hardware every time it's context switched in, after that counter exceeds this value
 * we'll just proceed to always save/restore the state because we assume the thread will likely
 * use the FPU again.
 *
 * @note The offsets of these values are critical! They're used from the assembly routines.
*/
struct ThreadState {
    /// Stack pointer to return to
    void *stackTop = nullptr;

    /// when set, FPU state should be saved and restored
    bool fpuEnabled = false;
    /// pointer to FPU data area (must be 16 byte aligned)
    void *fxsave = nullptr;

    /// number of times we've taken an FPU fault in this thread
    size_t fpuFaults = 0;

    /// FS base for the thread
    uintptr_t fsBase = 0;
    /// GS Base for the thread
    uintptr_t gsBase = 0;
};

/**
 * Task state specific to an x86 processor
 */
struct TaskState {
    // nothing 
};

/**
 * Registers for a kernel thread as saved on the stack.
 */
struct CpuRegs {
    // registers added for 64-bit mode
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    // basic registers (in common with 32-bit mode, in same order as PUSHA)
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    // CPU flags
    uint64_t rflags;
    // instruction pointer/return address
    uint64_t rip;
} __attribute__((packed));
}

// ensure the manually defined offsets are right
static_assert(offsetof(arch::ThreadState, stackTop) == TS_OFF_STACKTOP, "TS_OFF_STACKTOP wrong");
static_assert(offsetof(arch::ThreadState, fpuEnabled) == TS_OFF_FPU_ENABLED,
        "TS_OFF_FPU_ENABLED wrong");
static_assert(offsetof(arch::ThreadState, fxsave) == TS_OFF_FXSAVE, "TS_OFF_FXSAVE wrong");

#endif // ASM_FILE
#endif
