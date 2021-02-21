#include <sys/syscalls.h>
#include <stdint.h>

#include <stdlib.h>
#include <stdio.h>

/**
 * Aborts program execution with an abnormal error code.
 */
void abort() {
    fprintf(stderr, "abort()\n");
    TaskExit(0, -1);

    // ensure we really, really die
    asm volatile("ud2");
    while(1) {};
}
