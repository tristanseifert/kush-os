#ifndef LIBC_SYSCALLS_H
#define LIBC_SYSCALLS_H

#include <_libc.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

LIBC_EXPORT int AllocVirtualRegion(const uint64_t physAddr, const uintptr_t virtualAddr,
        const uintptr_t size, const uintptr_t inFlags, uintptr_t *outHandle);

LIBC_EXPORT int ThreadGetHandle(uintptr_t *outHandle);
LIBC_EXPORT int ThreadYield();
LIBC_EXPORT int ThreadUsleep(const uintptr_t usecs);
LIBC_EXPORT int ThreadCreate(void (*entry)(uintptr_t), const uintptr_t entryArg,
        const uintptr_t stack, uintptr_t *outHandle);
LIBC_EXPORT int ThreadDestroy(const uintptr_t handle);
LIBC_EXPORT int ThreadSetPriority(const uintptr_t handle, const int priority);
LIBC_EXPORT int ThreadSetName(const uintptr_t handle, const char *name);



LIBC_EXPORT int TaskGetHandle(uintptr_t *outHandle);
LIBC_EXPORT int TaskExit(const uintptr_t handle, const uintptr_t returnCode);
LIBC_EXPORT int TaskSetName(const uintptr_t handle, const char *name);

LIBC_EXPORT int DbgOut(const char *string, const size_t length);

#ifdef __cplusplus
}
#endif

/**
 * Flags for AllocVirtualRegion
 */
#define VM_REGION_FORCE_ALLOC           (1 << 0)
#define VM_REGION_USE_LARGEPAGE         (1 << 1)
#define VM_REGION_READ                  (1 << 10)
#define VM_REGION_WRITE                 (1 << 11)
#define VM_REGION_EXEC                  (1 << 12)
#define VM_REGION_MMIO                  (1 << 13)
#define VM_REGION_WRITETHRU             (1 << 14)
#define VM_REGION_NOMAP                 (1 << 15)

#define VM_REGION_RW                    (VM_REGION_READ | VM_REGION_WRITE)

#endif
