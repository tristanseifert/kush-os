#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>

/**
 * Terminates the specified process.
 */
int ProcessExit(const uintptr_t handle, const uintptr_t code) {
    return __do_syscall2(SYS_TASK_TERMINATE, handle, code);
}
