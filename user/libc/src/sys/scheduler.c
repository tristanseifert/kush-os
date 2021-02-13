#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>

/**
 * Returns the current thread's handle.
 */
int ThreadGetHandle() {
    return __do_syscall0(SYS_THREAD_GET_HANDLE);
}

/**
 * Gives up the remainder of the thread's CPU quantum.
 */
int ThreadYield() {
    return __do_syscall0(SYS_THREAD_YIELD);
}

/**
 * Sleeps for the given number of microseconds.
 */
int ThreadUsleep(const uintptr_t usecs) {
    return __do_syscall1(SYS_THREAD_SLEEP, usecs);
}
