#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>

/**
 * Aborts program execution with an abnormal error code.
 */
void abort() {
    // TODO: a much better implementation
    asm volatile("ud2");
    while(1) {};
}
