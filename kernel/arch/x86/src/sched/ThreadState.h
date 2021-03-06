#ifndef ARCH_X86_THREADSTATE_H
#define ARCH_X86_THREADSTATE_H

// we have to manually define structs offsets for our assembler code
#define TS_OFF_STACKTOP                 0
#define TS_OFF_FPU_ENABLED              4
#define TS_OFF_FXSAVE                   8

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

    /// base address to update the %gs segment with on context switch
    uintptr_t gsBase = 0;
};

/**
 * Task state specific to an x86 processor
 *
 * This is mainly used to handle IO permissions bitmaps.
 */
struct TaskState {
    /// when set, we have a custom TSS for the task
    bool hasTss = false;
    /// index of the corresponding TSS
    uintptr_t tssIdx = 0;
    /// Most significant accessible IO port, or 0 if none
    uintptr_t iopbBits = 0;

    /// Release the TSS when deallocated
    ~TaskState();
};

/**
 * Registers for a kernel thread as saved on the stack.
 */
struct CpuRegs {
    // segment selectors
    uint32_t gs, fs, es, ds;
    // registers from PUSHA; esp there is useless
    uint32_t edi, esi, ebp, oesp, ebx, edx, ecx, eax;
    // CPU flags
    uint32_t eflags;
    // and the instruction pointer/return address. kernel always uses the same code seg
    uint32_t eip;
} __attribute__((packed));
}

// ensure the manually defined offsets are right
static_assert(offsetof(arch::ThreadState, stackTop) == TS_OFF_STACKTOP, "TS_OFF_STACKTOP wrong");
static_assert(offsetof(arch::ThreadState, fpuEnabled) == TS_OFF_FPU_ENABLED,
        "TS_OFF_FPU_ENABLED wrong");
static_assert(offsetof(arch::ThreadState, fxsave) == TS_OFF_FXSAVE, "TS_OFF_FXSAVE wrong");

#endif // ASM_FILE
#endif
