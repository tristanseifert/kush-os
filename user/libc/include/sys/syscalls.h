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
 * Terminates the specified process.
 *
 * A handle value of zero terminates the currently running task.
 */
LIBC_EXPORT int ProcessExit(const uintptr_t handle, const uintptr_t returnCode);

#ifdef __cplusplus
}
#endif

#endif
