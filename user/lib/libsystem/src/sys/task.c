#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>
#include <string.h>

/**
 * Creates a new task; its parent is the given task.
 *
 * The kernel may perform validation to ensure the caller has the rights to add child tasks to the
 * provided task handle. A task handle of 0 indicates the calling task.
 */
int TaskCreateWithParent(const uintptr_t parent, uintptr_t *outHandle) {
    if(!outHandle) {
        return -1;
    }

    intptr_t err = __do_syscall1(SYS_TASK_CREATE, parent);

    if(err > 0) {
        *outHandle = err;
        return 0;
    } else {
        return err;
    }
}

/**
 * Creates a new task with the caller as its parent.
 */
int TaskCreate(uintptr_t *outHandle) {
    return TaskCreateWithParent(0, outHandle);
}

/**
 * Executes a return to usermode in the given task's main thread.
 */
int TaskInitialize(const uintptr_t taskHandle, const uintptr_t pc, const uintptr_t sp) {
    return __do_syscall3(SYS_TASK_INIT, taskHandle, pc, sp);
}

/**
 * Returns the current task's handle.
 */
int TaskGetHandle(uintptr_t *outHandle) {
    intptr_t err = __do_syscall0(SYS_TASK_GET_HANDLE);

    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
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
