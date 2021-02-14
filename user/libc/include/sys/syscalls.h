#ifndef LIBC_SYSCALLS_H
#define LIBC_SYSCALLS_H

#include <_libc.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Gets the thread handle for the currently executing thread.
 *
 * @return Thread handle (>0) or error code
 */
LIBC_EXPORT int ThreadGetHandle();

/**
 * Gives up the remainder of the thread's processor time.
 */
LIBC_EXPORT int ThreadYield();

/**
 * Sleeps for the given number of microseconds.
 */
LIBC_EXPORT int ThreadUsleep(const uintptr_t usecs);

/**
 * Sets the name of the current thread.
 */
LIBC_EXPORT int ThreadSetName(const uintptr_t handle, const char *name);



/**
 * Gets the task handle for the currently executing task.
 *
 * @return Task handle (>0) or error code
 */
LIBC_EXPORT int TaskGetHandle();

/**
 * Terminates the specified task.
 *
 * A handle value of zero terminates the currently running task.
 */
LIBC_EXPORT int TaskExit(const uintptr_t handle, const uintptr_t returnCode);

/**
 * Sets the name of the current task.
 */
LIBC_EXPORT int TaskSetName(const uintptr_t handle, const char *name);



/**
 * Writes the given string to the debug output stream for the process.
 */
LIBC_EXPORT int DbgOut(const char *string, const size_t length);

#ifdef __cplusplus
}
#endif

#endif
