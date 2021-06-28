#include "syscall.h"
#include "helpers.h"
#include <sys/syscalls.h>
#include <stdbool.h>
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
 * Structure for the SYS_VM_MAP_EX syscall
 */
struct VmMapRequest {
    /// start and end of the range to search for mapping. start is written with actual address
    uintptr_t start, end;
    /// length of the view
    size_t length;
    /// any mapping flags (standard syscall flag meaning)
    uintptr_t flags;
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
static uintptr_t BuildSyscallFlags(const uintptr_t inFlags, bool create) {
    uintptr_t temp = 0;

    if(create && inFlags & VM_REGION_FORCE_ALLOC) {
        temp |= (1 << 0);
    }
    if(create && (inFlags & VM_REGION_LOCKED)) {
        temp |= (1 << 8);
    }

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

    return temp;
}
/**
 * Builds the flag value to be provided to the various system calls to map a virtual memory object
 * into memory.
 *
 * @note If _any_ of the permission (RWX) flags are specified, the combination of these flags is
 *       used as a mask on the VM object's base permission flags. This means that it's never
 *       possible to make an explicitly read-only object writeable.
 *
 * @param inFlags User provided flags (from the VM_MAP_* constants)
 * @param remote When set, flags relating to remote mappings are allowed
 */
static uintptr_t BuildSyscallMapFlags(const uintptr_t inFlags, bool remote) {
    uintptr_t temp = 0;

    // permissions for new mapping
    if(inFlags & VM_MAP_READ) {
        temp |= (1 << 10);
    }
    if(inFlags & VM_MAP_WRITE) {
        temp |= (1 << 11);
    }
    if(inFlags & VM_MAP_EXEC) {
        temp |= (1 << 12);
    }

    // remote flags
    if(remote) {
        if(inFlags & VM_MAP_ADOPT) {
            temp |= (1 << 24);
        }
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
int AllocVirtualAnonRegion(const uintptr_t size, const uintptr_t inFlags, uintptr_t *outHandle) {
    intptr_t err;
    if(!outHandle) return -1;

    // build flags
    uintptr_t flags = 0;
    flags = BuildSyscallFlags(inFlags, true);

    // perform syscall
    err = __do_syscall2(size, flags, SYS_VM_CREATE_ANON);

    // return 0 for success, syscall error otherwise
    if(err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

/**
 * Creates a new virtual memory mapping backed by a contiguous range of physical addresses.
 */
int AllocVirtualPhysRegion(const uint64_t physAddr, const uintptr_t size, const uintptr_t inFlags,
        uintptr_t *outHandle) {
    intptr_t err;
    if(!outHandle) return -1;

    // build flags
    uintptr_t flags = 0;
    flags = BuildSyscallFlags(inFlags, true);

    // perform syscall
    err = __do_syscall3(physAddr, size, flags, SYS_VM_CREATE);

    // return 0 for success, syscall error otherwise
    if(err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

/**
 * Deallocates a virtual memory object.
 */
int DeallocVirtualRegion(const uintptr_t handle) {
    return __do_syscall1(handle, SYS_VM_DEALLOC);
}

/**
 * Resizes the provided VM region.
 */
int ResizeVirtualRegion(const uintptr_t regionHandle, const uintptr_t newSize) {
    return __do_syscall3(regionHandle, newSize, 0, SYS_VM_RESIZE);
}

/**
 * Unmaps a virtual memory region from the current task.
 */
int UnmapVirtualRegion(const uintptr_t handle) {
    return __do_syscall2(handle, 0, SYS_VM_UNMAP);
}

/**
 * Unmaps a virtual memory region from the specified task.
 */
int UnmapVirtualRegionFrom(const uintptr_t regionHandle, const uintptr_t taskHandle) {
    return __do_syscall2(regionHandle, taskHandle, SYS_VM_UNMAP);
}

/**
 * Maps a virtual memory region in the current task.
 */
int MapVirtualRegion(const uintptr_t regionHandle, const uintptr_t base, const size_t length,
        const uintptr_t inFlags) {
    // validate args
    if(!regionHandle || !base) return -1;

    // perform the syscall
    const uintptr_t flags = BuildSyscallMapFlags(inFlags, false);
    return __do_syscall5(regionHandle, 0, base, length, flags, SYS_VM_MAP);
}

/**
 * Maps a virtual memory region in another task.
 *
 * @param taskHandle Task to map the object into; passing 0 here is a library use error
 */
int MapVirtualRegionRemote(const uintptr_t taskHandle, const uintptr_t regionHandle,
        const uintptr_t base, const size_t length, const uintptr_t inFlags) {
    // validate args
    if(!taskHandle || !regionHandle || !base) return -1;

    // perform the syscall
    const uintptr_t flags = BuildSyscallMapFlags(inFlags, true);
    return __do_syscall5(regionHandle, taskHandle, base, length, flags, SYS_VM_MAP);
}

/**
 * Searches for a free space of virtual memory big enough to fit a view of the specified length,
 * which maps the given VM region.
 *
 * @param range An array containing the starting and ending addresses of the range
 * @param outBase If non-null, a location into which the base address of the mapping is written
 */
int MapVirtualRegionRange(const uintptr_t regionHandle, const uintptr_t range[2],
        const size_t length, const uintptr_t inFlags, uintptr_t *outBase) {
    int err;

    // validate args
    if(!regionHandle || !range || !length) return -1;
    else if(!range[0]) return -1;

    // build the request
    struct VmMapRequest req = {
        .start  = range[0],
        .end    = range[1],
        .length = length,
        .flags  = BuildSyscallMapFlags(inFlags, false)
    };

    // invoke method
    err = __do_syscall4(regionHandle, 0, (const uintptr_t) &req, sizeof(req), SYS_VM_MAP_EX);

    if(!err) {
        // the kernel will write the actual base address into `start`
        if(outBase) *outBase = req.start;
    } else {
        // clear the address, as we can't have a mapping at address 0
        if(outBase) *outBase = 0;
    }

    return err;
}

/**
 * Searches for a free space of virtual memory big enough to fit a view of the specified length,
 * which maps the given VM region in a remote task.
 *
 * @param taskHandle Task into which the object is mapped
 * @param range An array containing the starting and ending addresses of the range, in the remote
 *        task's address space
 * @param outBase If non-null, a location into which the base address of the mapping is written
 */
int MapVirtualRegionRangeRemote(const uintptr_t taskHandle, const uintptr_t regionHandle,
        const uintptr_t range[2], const size_t length, const uintptr_t inFlags,
        uintptr_t *outBase) {
    int err;

    // validate args
    if(!taskHandle || !regionHandle || !range || !length) return -1;
    else if(!range[0]) return -1;

    // build the request
    struct VmMapRequest req = {
        .start  = range[0],
        .end    = range[1],
        .length = length,
        .flags  = BuildSyscallMapFlags(inFlags, true)
    };

    // invoke method
    err = __do_syscall4(regionHandle, taskHandle, (const uintptr_t) &req, sizeof(req),
            SYS_VM_MAP_EX);

    if(!err) {
        // the kernel will write the actual base address into `start`
        if(outBase) *outBase = req.start;
    } else {
        // clear the address, as we can't have a mapping at address 0
        if(outBase) *outBase = 0;
    }

    return err;
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
    InternalMemset(&info, 0, sizeof(info));

    err = __do_syscall4(regionHandle, taskHandle, (uintptr_t) &info, sizeof(info),
            SYS_VM_GET_INFO);

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
    return __do_syscall3(taskHandle, (uintptr_t) info, infoSz, 
            SYS_VM_GET_TASK_INFO);
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
    err = __do_syscall2(taskHandle, address, SYS_VM_ADDR_TO_HANDLE);
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
    const uintptr_t flags = BuildSyscallFlags(newFlags, false);
    return __do_syscall2(regionHandle, flags, SYS_VM_UPDATE_FLAGS);
}

/**
 * Translates a set of virtual addresses (in the current task's address space) to physical
 * addresses. The input addresses are read from one array, while the output addresses are written
 * to another, or unmodified if the virtual address could not be translated.
 */
int VirtualToPhysicalAddr(const uintptr_t *virtualAddrs, const size_t numAddrs,
        uintptr_t *outPhysAddrs) {
    // super basic validation
    if(!virtualAddrs || !numAddrs || !outPhysAddrs) return -1;

    return __do_syscall4(0, (uintptr_t) virtualAddrs, (uintptr_t) outPhysAddrs, numAddrs,
            SYS_VM_VIRT_TO_PHYS);
}

/**
 * Performs a request into the kernel to get some information from the virtual memory subsystem.
 */
int QueryVirtualParams(const VirtualParams_t what, void *outPtr, const size_t outBytes) {
    return __do_syscall3(what, (uintptr_t) outPtr, outBytes, SYS_VM_QUERY);
}
