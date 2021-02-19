#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>

/**
 * Aborts program execution with an abnormal error code.
 */
void abort() {
    TaskExit(0, -1);

    // ensure we really, really die
    asm volatile("ud2");
    while(1) {};
}
