#ifndef _LIBSYSTEM_SYSCALLS_THREAD_H
#define _LIBSYSTEM_SYSCALLS_THREAD_H

#include <_libsystem.h>
#include <stdint.h>

LIBSYSTEM_EXPORT int ThreadGetHandle(uintptr_t *outHandle);
LIBSYSTEM_EXPORT int ThreadYield();
LIBSYSTEM_EXPORT int ThreadUsleep(const uintptr_t usecs);
LIBSYSTEM_EXPORT int ThreadCreate(void (*entry)(uintptr_t), const uintptr_t entryArg,
        const uintptr_t stack, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int ThreadDestroy(const uintptr_t handle);
LIBSYSTEM_EXPORT int ThreadSetPriority(const uintptr_t handle, const int priority);
LIBSYSTEM_EXPORT int ThreadSetName(const uintptr_t handle, const char *name);


#endif
