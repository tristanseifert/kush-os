#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>
#include <string.h>

extern void __ThreadStackPrepare(const uintptr_t stack, void (*entry)(uintptr_t), const uintptr_t arg);
extern void __ThreadTrampoline();

/**
 * Returns the current thread's handle.
 */
int ThreadGetHandle(uintptr_t *outHandle) {
    int err = __do_syscall0(SYS_THREAD_GET_HANDLE);

    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
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

/**
 * Creates a new userspace thread.
 *
 * This goes through a small trampoline which prepares the thread's state, based on information we
 * shove on to the stack.
 *
 * @return Handle to the new thread (> 0) or an error code.
 */
int ThreadCreate(void (*entry)(uintptr_t), const uintptr_t entryArg, const uintptr_t stack,
        uintptr_t *outHandle) {
    // prepare the stack environment
    __ThreadStackPrepare(stack, entry, entryArg);

    // create the thread
    int err = __do_syscall3(SYS_THREAD_CREATE, (uintptr_t) &__ThreadTrampoline, 0, stack);

    // extract handle if desired; return 0 for success, syscall error otherwise
    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

/**
 * Destroys the specified thread. It will be deleted immediately if not running, or on the next
 * trip into the kernel otherwise. If it's the currently running thread however, it will be deleted
 * immediately.
 *
 * @param handle Thread handle, or 0 for the current thread
 */
int ThreadDestroy(const uintptr_t handle) {
    return __do_syscall1(SYS_THREAD_DESTROY, handle);
}

/**
 * Updates the thread's priority value.
 *
 * @param thread Thread handle, or 0 for the current thread.
 */
int ThreadSetPriority(const uintptr_t handle, const int priority) {
    return __do_syscall2(SYS_THREAD_SET_PRIORITY, handle, (uintptr_t) priority);
}

/**
 * Set the name of the thread whose handle is given.
 *
 * @param thread Thread handle, or 0 for the current thread.
 * @param namePtr Pointer to a NULL terminated UTF-8 string.
 */
int ThreadSetName(const uintptr_t handle, const char *name) {
    const size_t nameLen = strlen(name);
    return __do_syscall3(SYS_THREAD_RENAME, handle, (uintptr_t) name, nameLen);
}