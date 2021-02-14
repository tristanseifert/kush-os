#ifndef LIBC_SYSCALLS_H
#define LIBC_SYSCALLS_H

#include <_libc.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

LIBC_EXPORT int ThreadGetHandle();
LIBC_EXPORT int ThreadYield();
LIBC_EXPORT int ThreadUsleep(const uintptr_t usecs);
LIBC_EXPORT int ThreadCreate(void (*entry)(uintptr_t), const uintptr_t entryArg, const uintptr_t stack);
LIBC_EXPORT int ThreadDestroy(const uintptr_t handle);
LIBC_EXPORT int ThreadSetPriority(const uintptr_t handle, const int priority);
LIBC_EXPORT int ThreadSetName(const uintptr_t handle, const char *name);



LIBC_EXPORT int TaskGetHandle();
LIBC_EXPORT int TaskExit(const uintptr_t handle, const uintptr_t returnCode);
LIBC_EXPORT int TaskSetName(const uintptr_t handle, const char *name);

LIBC_EXPORT int DbgOut(const char *string, const size_t length);

#ifdef __cplusplus
}
#endif

#endif
