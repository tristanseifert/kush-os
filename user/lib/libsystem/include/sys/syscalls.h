#ifndef LIBSYSTEM_SYSCALLS_H
#define LIBSYSTEM_SYSCALLS_H

#include <_libsystem.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MessageHeader {
    /// handle of the thread that sent this message
    uintptr_t sender;
    /// flags (not currently used)
    uint16_t flags;
    /// number of bytes of message data
    uint16_t receivedBytes;

    uintptr_t reserved[2];

    /// message data
    uint8_t data[];
} __attribute__((aligned(16)));

LIBSYSTEM_EXPORT int PortCreate(uintptr_t *outHandle);
LIBSYSTEM_EXPORT int PortDestroy(const uintptr_t portHandle);
LIBSYSTEM_EXPORT int PortSend(const uintptr_t portHandle, const void *message, const size_t messageLen);
LIBSYSTEM_EXPORT int PortReceive(const uintptr_t portHandle, struct MessageHeader *buf,
        const size_t bufMaxLen, const uintptr_t blockUs);
LIBSYSTEM_EXPORT int PortSetQueueDepth(const uintptr_t portHandle, const uintptr_t queueDepth);

LIBSYSTEM_EXPORT int AllocVirtualAnonRegion(const uintptr_t virtualAddr, const uintptr_t size,
        const uintptr_t inFlags, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int AllocVirtualRegion(const uint64_t physAddr, const uintptr_t virtualAddr,
        const uintptr_t size, const uintptr_t inFlags, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int MapVirtualRegion(const uintptr_t regionHandle);
LIBSYSTEM_EXPORT int MapVirtualRegionAt(const uintptr_t regionHandle, const uintptr_t baseAddr);
LIBSYSTEM_EXPORT int MapVirtualRegionTo(const uintptr_t regionHandle, const uintptr_t taskHandle);
LIBSYSTEM_EXPORT int MapVirtualRegionAtTo(const uintptr_t regionHandle, const uintptr_t taskHandle,
        const uintptr_t baseAddr);
LIBSYSTEM_EXPORT int UnmapVirtualRegion(const uintptr_t regionHandle);
LIBSYSTEM_EXPORT int UnmapVirtualRegionFrom(const uintptr_t regionHandle, const uintptr_t taskHandle);
LIBSYSTEM_EXPORT int VirtualRegionGetInfo(const uintptr_t regionHandle, uintptr_t *baseAddr,
        uintptr_t *length, uintptr_t *flags);
LIBSYSTEM_EXPORT int VirtualRegionGetInfoFor(const uintptr_t regionHandle, const uintptr_t task,
        uintptr_t *baseAddr, uintptr_t *length, uintptr_t *flags);

LIBSYSTEM_EXPORT int ThreadGetHandle(uintptr_t *outHandle);
LIBSYSTEM_EXPORT int ThreadYield();
LIBSYSTEM_EXPORT int ThreadUsleep(const uintptr_t usecs);
LIBSYSTEM_EXPORT int ThreadCreate(void (*entry)(uintptr_t), const uintptr_t entryArg,
        const uintptr_t stack, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int ThreadDestroy(const uintptr_t handle);
LIBSYSTEM_EXPORT int ThreadSetPriority(const uintptr_t handle, const int priority);
LIBSYSTEM_EXPORT int ThreadSetName(const uintptr_t handle, const char *name);



LIBSYSTEM_EXPORT int TaskGetHandle(uintptr_t *outHandle);
LIBSYSTEM_EXPORT int TaskExit(const uintptr_t handle, const uintptr_t returnCode);
LIBSYSTEM_EXPORT int TaskSetName(const uintptr_t handle, const char *name);

LIBSYSTEM_EXPORT int DbgOut(const char *string, const size_t length);

#ifdef __cplusplus
}
#endif

/**
 * Flags for AllocVirtualRegion
 */
#define VM_REGION_FORCE_ALLOC           (1 << 0)
#define VM_REGION_USE_LARGEPAGE         (1 << 1)
#define VM_REGION_ANON                  (1 << 7)
#define VM_REGION_READ                  (1 << 10)
#define VM_REGION_WRITE                 (1 << 11)
#define VM_REGION_EXEC                  (1 << 12)
#define VM_REGION_MMIO                  (1 << 13)
#define VM_REGION_WRITETHRU             (1 << 14)
#define VM_REGION_NOMAP                 (1 << 15)

#define VM_REGION_RW                    (VM_REGION_READ | VM_REGION_WRITE)

#endif
