#ifndef _LIBSYSTEM_SYSCALLS_TASK_H
#define _LIBSYSTEM_SYSCALLS_TASK_H

#include <_libsystem.h>
#include <stddef.h>
#include <stdint.h>

LIBSYSTEM_EXPORT int TaskGetHandle(uintptr_t *outHandle);
LIBSYSTEM_EXPORT int TaskExit(const uintptr_t handle, const uintptr_t returnCode);
LIBSYSTEM_EXPORT int TaskSetName(const uintptr_t handle, const char *name);

LIBSYSTEM_EXPORT int DbgOut(const char *string, const size_t length);

#endif
