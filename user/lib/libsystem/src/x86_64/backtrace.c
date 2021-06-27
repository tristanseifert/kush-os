#include "sys/backtrace.h"

#include <stdio.h>
#include <stdint.h>

/// amd64 stack frame format
struct stackframe {
    struct stackframe *rbp;
    uint64_t rip;
};

int BacktracePrint(void *stack, char *buf, const size_t bufLen) {
    struct stackframe *stk;

    if(!stack) {
        asm volatile("movq %%rbp,%0" : "=r"(stk) ::);
    } else {
        stk = (struct stackframe *) stack;
    }

    // yeet out into the buffer the frames
    char *writePtr = buf;
    int written;

    for(size_t frame = 0; stk && frame < 50; ++frame) {
        const size_t bufAvail = bufLen - (writePtr - buf);
        if(!bufAvail) return bufLen;

        written = snprintf(writePtr, bufAvail, "%3zu: %p\n", frame, (void *) stk->rip);

        // prepare for next frame
        writePtr += written;
        stk = stk->rbp;
    }

    // return bytes written
    return writePtr - buf;
}
