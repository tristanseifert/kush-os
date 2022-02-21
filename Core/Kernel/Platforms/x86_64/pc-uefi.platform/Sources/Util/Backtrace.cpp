#include "Backtrace.h"

#include <Runtime/Printf.h>
#include <stdint.h>

using namespace Platform::Amd64Uefi;

/// x86_64 stack frame
struct stackframe {
    struct stackframe *rbp;
    uint64_t rip;
};

/**
 * Prints a backtrace to the given character buffer.
 *
 * @param stack Stack base pointer to start at, or `nullptr` to use the current one
 * @param outBuf Character buffer to write data to
 * @param outBufLen Number of characters the output buffer has space for
 * @param symbolicate Whether we should try to resolve addresses to function names
 *
 * @return Number of stack frames output
 */
int Backtrace::Print(const void *stack, char *outBuf, const size_t outBufLen,
        const bool symbolicate) {
    int numFrames{0};

    // get the initial stack frame
    const struct stackframe *stk;

    if(!stack) {
        asm volatile("movq %%rbp,%0" : "=r"(stk) ::);
    } else {
        stk = reinterpret_cast<const struct stackframe *>(stack);
    }

    if(!stk) return 0;

    // walk the stack
    char *writePtr = outBuf;
    int written;

    for(size_t frame = 0; stk && frame < 50; ++frame) {
        const auto bufAvail = outBufLen - (writePtr - outBuf);
        if(!bufAvail) goto full;

        // skip if return address is null
        if(!stk->rip) goto next;

        // symbolicate, if requested
        if(symbolicate) {
            written = snprintf_(writePtr, bufAvail, "%2zu %016llx\n", frame, stk->rip);
        }
        // print raw address
        else {
            written = snprintf_(writePtr, bufAvail, "%2zu %016llx\n", frame, stk->rip);
        }

next:
        // prepare for next frame
        writePtr += written;
        stk = stk->rbp;
        numFrames++;

        // bail if this frame is invalid
        if(!stk || !(reinterpret_cast<uintptr_t>(stk) & (1UL << 63))) goto full;
    }

full:
    return numFrames;
}

