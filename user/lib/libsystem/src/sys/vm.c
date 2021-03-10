#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>
#include <string.h>

/**
 * Info structure for the "get VM region info" syscall
 */
struct VmInfoStruct {
    // base address of the region
    uintptr_t virtualBase;
    // length of the region in bytes
    uintptr_t length;

    uint16_t reserved;
    // region flags: this is the same flags as passed to the syscall
    uint16_t flags;
};

/**
 * Builds a syscall flag value.
 *
 * You may notice this is the same as the ones defined in the header; therefore no real translation
 * is taking place here. This allows us flexibility to change the flags without changing the actual
 * programs later.
 *
 * @param create When set, extra flags allowed only for VM region creation are produced. Otherwise,
 * they are simply ignored.
 */
static uintptr_t BuildSyscallFlags(const uintptr_t inFlags, int create) {
    uintptr_t temp = 0;

    if(inFlags & VM_REGION_READ) {
        temp |= (1 << 10);
    }
    if(inFlags & VM_REGION_WRITE) {
        temp |= (1 << 11);
    }
    if(inFlags & VM_REGION_EXEC) {
        temp |= (1 << 12);
    }
    if(inFlags & VM_REGION_MMIO) {
        temp |= (1 << 13);
    }
    if(inFlags & VM_REGION_WRITETHRU) {
        temp |= (1 << 14);
    }
    if(create && inFlags & VM_REGION_NOMAP) {
        temp |= (1 << 15);
    }

    return temp;
}
/**
 * Converts syscall flags back into our flag values.
 */
static uintptr_t ConvertSyscallFlags(const uintptr_t inFlags) {
    uintptr_t temp = 0;

    if(inFlags & (1 << 7)) {
        temp |= VM_REGION_ANON;
    }
    if(inFlags & (1 << 10)) {
        temp |= VM_REGION_READ;
    }
    if(inFlags & (1 << 11)) {
        temp |= VM_REGION_WRITE;
    }
    if(inFlags & (1 << 12)) {
        temp |= VM_REGION_EXEC;
    }
    if(inFlags & (1 << 13)) {
        temp |= VM_REGION_MMIO;
    }
    if(inFlags & (1 << 14)) {
        temp |= VM_REGION_WRITETHRU;
    }


    return temp;
}


/**
 * Creates a new virtual memory mapping backed by anonymous memory.
 */
int AllocVirtualAnonRegion(const uintptr_t virtualAddr, const uintptr_t size,
        const uintptr_t inFlags, uintptr_t *outHandle) {
    intptr_t err;

    // build flags
    uintptr_t flags = 0;
    flags = BuildSyscallFlags(inFlags, 1);

    // perform syscall
    err = __do_syscall2(SYS_VM_CREATE_ANON | (flags << 16), virtualAddr, size);

    // extract handle if desired; return 0 for success, syscall error otherwise
    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

/**
 * Creates a new virtual memory mapping backed by a contiguous range of physical addresses.
 */
int AllocVirtualRegion(const uint64_t physAddr, const uintptr_t virtualAddr, const uintptr_t size,
        const uintptr_t inFlags, uintptr_t *outHandle) {
    intptr_t err;

    // build flags
    uintptr_t flags = 0;
    flags = BuildSyscallFlags(inFlags, 1);

    // perform syscall
    err = __do_syscall3(SYS_VM_CREATE | (flags << 16), physAddr, virtualAddr, size);

    // extract handle if desired; return 0 for success, syscall error otherwise
    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

/**
 * Resizes the provided VM region.
 */
int ResizeVirtualRegion(const uintptr_t regionHandle, const uintptr_t newSize) {
    return __do_syscall2(SYS_VM_RESIZE, regionHandle, newSize);
}

/**
 * Unmaps a virtual memory region from the current task.
 */
int UnmapVirtualRegion(const uintptr_t handle) {
    return __do_syscall2(SYS_VM_UNMAP, handle, 0);
}

/**
 * Unmaps a virtual memory region from the specified task.
 */
int UnmapVirtualRegionFrom(const uintptr_t regionHandle, const uintptr_t taskHandle) {
    return __do_syscall2(SYS_VM_UNMAP, regionHandle, taskHandle);
}

/**
 * Maps a virtual memory region from the current task.
 */
int MapVirtualRegion(const uintptr_t handle) {
    return __do_syscall3(SYS_VM_MAP, handle, 0, 0);
}

/**
 * Maps a virtual memory region from the current task.
 */
int MapVirtualRegionAt(const uintptr_t handle, const uintptr_t baseAddr) {
    return __do_syscall3(SYS_VM_MAP, handle, 0, baseAddr);
}

/**
 * Maps a virtual memory region from the specified task.
 */
int MapVirtualRegionTo(const uintptr_t regionHandle, const uintptr_t taskHandle) {
    return __do_syscall3(SYS_VM_MAP, regionHandle, taskHandle, 0);
}
/**
 * Maps a virtual memory region from the specified task, with a specified flags mask.
 */
int MapVirtualRegionToFlags(const uintptr_t regionHandle, const uintptr_t taskHandle, const uintptr_t flagsMask) {
    uintptr_t flags = 0;
    flags = BuildSyscallFlags(flagsMask, 0);

    return __do_syscall3(SYS_VM_MAP | ((flags & 0xFFFF) << 16), regionHandle, taskHandle, 0);
}

/**
 * Maps a virtual memory region into the specified task.
 */
int MapVirtualRegionAtTo(const uintptr_t regionHandle, const uintptr_t taskHandle,
        const uintptr_t baseAddr) {
    return __do_syscall3(SYS_VM_MAP, regionHandle, taskHandle, baseAddr);
}



/**
 * Gets info on an existing virtual memory region, as seen by the current task.
 */
int VirtualRegionGetInfo(const uintptr_t regionHandle, uintptr_t *baseAddr, uintptr_t *length,
        uintptr_t *flags) {
    return VirtualRegionGetInfoFor(regionHandle, 0, baseAddr, length, flags);
}

/**
 * Gets info on an existing virtual memory region from the perspective of the given task. If the
 * task does not map the given region, an error is returned.
 *
 * @param regionHandle Handle to a virtual memory region
 * @param task Handle to a task, or 0 to reference the current task
 */
int VirtualRegionGetInfoFor(const uintptr_t regionHandle, const uintptr_t taskHandle,
        uintptr_t *baseAddr, uintptr_t *length, uintptr_t *flags) {
    intptr_t err;

    // make the call to populate temporary struct
    struct VmInfoStruct info;
    memset(&info, 0, sizeof(info));

    err = __do_syscall4(SYS_VM_GET_INFO, regionHandle, taskHandle, (uintptr_t) &info,
            sizeof(info));

    // bail immediately if error
    if(err != 0) {
        return err;
    }

    // otherwise, extract the fields
    if(baseAddr) *baseAddr = info.virtualBase;
    if(length) *length = info.length;
    if(flags) *flags = ConvertSyscallFlags(info.flags);

    return 0;
}



/**
 * Gets information about a task's virtual memory environment.
 */
int VirtualGetTaskInfo(const uintptr_t taskHandle, TaskVmInfo_t *info, const size_t infoSz) {
    return __do_syscall3(SYS_VM_GET_TASK_INFO, taskHandle, (uintptr_t) info, infoSz);
}

/**
 * Attempts to translate the given virtual address into a VM region handle for the current task.
 */
int VirtualGetHandleForAddr(const uintptr_t address, uintptr_t *regionHandle) {
    return VirtualGetHandleForAddrInTask(0, address, regionHandle);
}

/**
 * Translates the given virtual address, from the view of the given task, into a handle to the
 * region that contains that address.
 *
 * A successful return code does not indicate that the pages actually exist; just that there is a
 * mapping that is prepared to handle faults for nonexistent pages. Perform an info request for
 * this region for more information.
 *
 * @return Negative error code, 0 if there is no VM mapping at the given address, or 1 if there is
 * a region and its handle was written out.
 */
int VirtualGetHandleForAddrInTask(const uintptr_t taskHandle, const uintptr_t address,
        uintptr_t *regionHandle) {
    intptr_t err;

    // handle the error or non-existent mapping cases of the syscall
    err = __do_syscall2(SYS_VM_ADDR_TO_HANDLE, taskHandle, address);
    if(err < 0) {
        return err;
    } else if(!err) {
        return 0;
    }

    // otherwise, extract the fields
    if(regionHandle) *regionHandle = (uintptr_t) err;
    return 1;
}

/**
 * Updates the flags of a virtual memory region.
 *
 * Valid flags are VM_REGION_READ, VM_REGION_WRITE, VM_REGION_EXEC, VM_REGION_MMIO,
 * VM_REGION_WRITETHRU or a combination thereof.
 */
int VirtualRegionSetFlags(const uintptr_t regionHandle, const uintptr_t newFlags) {
    const uintptr_t flags = BuildSyscallFlags(newFlags, 0);
    return __do_syscall1(SYS_VM_UPDATE_FLAGS | ((flags & 0xFFFF) << 16), regionHandle);
}
