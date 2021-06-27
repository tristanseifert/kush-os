#include <sys/syscalls.h>
#include <sys/backtrace.h>
#include <stdint.h>

#include <stdlib.h>
#include <stdio.h>

/**
 * Aborts program execution with an abnormal error code.
 */
#define kBacktraceBufSz 2048

void abort() {
    static char backtraceBuf[kBacktraceBufSz];

    fprintf(stderr, "abort() called!\n");

    // generate a backtrace
    int err = BacktracePrint(NULL, backtraceBuf, kBacktraceBufSz);
    if(err) {
        fprintf(stderr, "Backtrace:\n%s", backtraceBuf);
    }


    TaskExit(0, -1);

    // ensure we really, really die
    asm volatile("ud2");
    while(1) {};
}
