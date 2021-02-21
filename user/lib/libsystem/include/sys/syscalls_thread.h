#ifndef _LIBSYSTEM_SYSCALLS_THREAD_H
#define _LIBSYSTEM_SYSCALLS_THREAD_H

#include <_libsystem.h>
#include <stdint.h>

LIBSYSTEM_EXPORT int ThreadGetHandle(uintptr_t *outHandle);
LIBSYSTEM_EXPORT int ThreadYield();
LIBSYSTEM_EXPORT int ThreadUsleep(const uintptr_t usecs);
LIBSYSTEM_EXPORT int ThreadCreateFlags(void (*entry)(uintptr_t), const uintptr_t entryArg,
        const uintptr_t stack, uintptr_t *outHandle, const uintptr_t flags);
LIBSYSTEM_EXPORT int ThreadCreate(void (*entry)(uintptr_t), const uintptr_t entryArg,
        const uintptr_t stack, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int ThreadDestroy(const uintptr_t handle);
LIBSYSTEM_EXPORT int ThreadSetPriority(const uintptr_t handle, const int priority);
LIBSYSTEM_EXPORT int ThreadSetName(const uintptr_t handle, const char *name);
LIBSYSTEM_EXPORT int ThreadWait(const uintptr_t threadHandle, const uintptr_t timeoutUsecs);
LIBSYSTEM_EXPORT int ThreadResume(const uintptr_t threadHandle);

/**
 * Flags for the ThreadCreate function
 */
/// The thread will not begin executing until a later call to ThreadResume().
#define THREAD_PAUSED                   (1 << 15)

#endif
