#ifndef _LIBSYSTEM_SYSCALLS_TASK_H
#define _LIBSYSTEM_SYSCALLS_TASK_H

#include <_libsystem.h>
#include <stddef.h>
#include <stdint.h>

LIBSYSTEM_EXPORT int TaskCreate(uintptr_t * _Nonnull outHandle);
LIBSYSTEM_EXPORT int TaskCreateWithParent(const uintptr_t parent, uintptr_t * _Nonnull outHandle);
LIBSYSTEM_EXPORT int TaskInitialize(const uintptr_t taskHandle, const uintptr_t pc, const uintptr_t sp);
LIBSYSTEM_EXPORT int TaskGetHandle(uintptr_t * _Nonnull outHandle);
LIBSYSTEM_EXPORT int TaskExit(const uintptr_t handle, const uintptr_t returnCode);
LIBSYSTEM_EXPORT int TaskSetName(const uintptr_t handle, const char * _Nonnull name);

LIBSYSTEM_EXPORT int DbgOut(const char * _Nonnull string, const size_t length);

#endif
