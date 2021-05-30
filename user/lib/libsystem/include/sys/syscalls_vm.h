#ifndef _LIBSYSTEM_SYSCALLS_VM_H
#define _LIBSYSTEM_SYSCALLS_VM_H

#include <_libsystem.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Information structure for the VirtualGetTaskInfo syscall
 */
typedef struct TaskVmInfo {
    /// total number of physical pages owned by the task
    uintptr_t numPagesOwned;
    /// total number of virtual memory mappings
    uintptr_t numVmMaps;
} TaskVmInfo_t;

/**
 * Flags for AllocVirtual*Region
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

/**
 * Flags for MapVirtualRegion*Remote
 */
#define VM_MAP_READ                     VM_REGION_READ
#define VM_MAP_WRITE                    VM_REGION_WRITE
#define VM_MAP_EXEC                     VM_REGION_EXEC

#define VM_MAP_ADOPT                    (1 << 24)


LIBSYSTEM_EXPORT int AllocVirtualAnonRegion(const uintptr_t size, const uintptr_t inFlags,
        uintptr_t *outHandle);
LIBSYSTEM_EXPORT int AllocVirtualPhysRegion(const uint64_t physAddr, const uintptr_t size,
        const uintptr_t inFlags, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int DeallocVirtualRegion(const uintptr_t regionHandle);

LIBSYSTEM_EXPORT int ResizeVirtualRegion(const uintptr_t regionHandle, const uintptr_t newSize);

LIBSYSTEM_EXPORT int MapVirtualRegion(const uintptr_t regionHandle, const uintptr_t baseAddr,
        const size_t length, const uintptr_t flags);
LIBSYSTEM_EXPORT int MapVirtualRegionRemote(const uintptr_t taskHandle,
        const uintptr_t regionHandle, const uintptr_t base, const size_t length,
        const uintptr_t flags);
LIBSYSTEM_EXPORT int MapVirtualRegionRange(const uintptr_t regionHandle, const uintptr_t range[2],
        const size_t length, const uintptr_t flags, uintptr_t *outBase);
LIBSYSTEM_EXPORT int MapVirtualRegionRangeRemote(const uintptr_t taskHandle, 
        const uintptr_t regionHandle, const uintptr_t range[2], const size_t length,
        const uintptr_t flags, uintptr_t *outBase);

LIBSYSTEM_EXPORT int UnmapVirtualRegion(const uintptr_t regionHandle);
LIBSYSTEM_EXPORT int UnmapVirtualRegionFrom(const uintptr_t regionHandle, const uintptr_t taskHandle);

LIBSYSTEM_EXPORT int VirtualRegionGetInfo(const uintptr_t regionHandle, uintptr_t *baseAddr,
        uintptr_t *length, uintptr_t *flags);
LIBSYSTEM_EXPORT int VirtualRegionGetInfoFor(const uintptr_t regionHandle, const uintptr_t task,
        uintptr_t *baseAddr, uintptr_t *length, uintptr_t *flags);
LIBSYSTEM_EXPORT int VirtualRegionSetFlags(const uintptr_t regionHandle, const uintptr_t newFlags);

LIBSYSTEM_EXPORT int VirtualGetTaskInfo(const uintptr_t taskHandle, TaskVmInfo_t *info,
        const size_t infoSize);

LIBSYSTEM_EXPORT int VirtualGetHandleForAddr(const uintptr_t address, uintptr_t *regionHandle);
LIBSYSTEM_EXPORT int VirtualGetHandleForAddrInTask(const uintptr_t taskHandle, 
        const uintptr_t address, uintptr_t *regionHandle);


#endif
