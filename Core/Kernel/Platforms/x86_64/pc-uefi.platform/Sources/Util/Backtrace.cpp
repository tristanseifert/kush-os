#include "Backtrace.h"
#include "Boot/Helpers.h"

#include <Runtime/Printf.h>
#include <stdint.h>

using namespace Platform::Amd64Uefi;

extern char __kernel_text_start, __kernel_text_end;

/// x86_64 stack frame
struct stackframe {
    struct stackframe *rbp;
    uint64_t rip;
};

/**
 * Initializes the backtrace facility.
 *
 * This attempts to locate the string and symbol tables in the loaded kernel ELF image.
 */
void Backtrace::Init(struct stivale2_struct *loaderInfo) {
    // TODO: implement
}

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

    constexpr static const size_t kSymbolNameBufLen{100};
    char symbolNameBuf[kSymbolNameBufLen];

    for(size_t frame = 0; stk && frame < 50; ++frame) {
        const auto bufAvail = outBufLen - (writePtr - outBuf);
        if(!bufAvail) goto full;

        // skip if return address is null
        if(!stk->rip) goto next;

        // symbolicate, if requested
        if(symbolicate) {
            written = Symbolicate(stk->rip, symbolNameBuf, kSymbolNameBufLen);
            if(written == 1) {
                written = snprintf_(writePtr, bufAvail, "\n%2zu %016llx %s", frame, stk->rip,
                        symbolNameBuf);
            }
            // regular peasant output
            else {
                goto beach;
            }

        }
        // print raw address
        else {
beach:;
            written = snprintf_(writePtr, bufAvail, "\n%2zu %016llx", frame, stk->rip);
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

/**
 * Attempt to symbolicate the provided symbol; it must be inside the kernel.
 *
 * @param pc Symbol address
 * @param outBuf Location where the symbol name is written
 * @param outBufLen Length of symbol name buffer
 *
 * @return 0 if no symbol found, -1 if an error occurred, and 1 if symbol was found.
 */
int Backtrace::Symbolicate(const uintptr_t pc, char *outBuf, const size_t outBufLen) {
    // ensure it's in the kernel .text section
    if(pc < reinterpret_cast<uintptr_t>(&__kernel_text_start) ||
            pc > reinterpret_cast<uintptr_t>(&__kernel_text_end)) {
        return 0;
    }

    // TODO: implement
    return -1;
}

