#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>
#include <string.h>

/**
 * Returns the current task's handle.
 */
int TaskGetHandle() {
    return __do_syscall0(SYS_TASK_GET_HANDLE);
}

/**
 * Terminates the specified task.
 */
int TaskExit(const uintptr_t handle, const uintptr_t code) {
    return __do_syscall2(SYS_TASK_TERMINATE, handle, code);
}

/**
 * Set the name of the task whose handle is given.
 *
 * @param thread Task handle, or 0 for the current task.
 * @param namePtr Pointer to a NULL terminated UTF-8 string.
 */
int TaskSetName(const uintptr_t handle, const char *name) {
    const size_t nameLen = strlen(name);
    return __do_syscall3(SYS_TASK_RENAME, handle, (uintptr_t) name, nameLen);
}



/**
 * Writes the given string to the debug output stream for the process.
 */
int DbgOut(const char *string, const size_t length) {
    return __do_syscall2(SYS_TASK_DBG_OUT, (uintptr_t) string, length);
}
