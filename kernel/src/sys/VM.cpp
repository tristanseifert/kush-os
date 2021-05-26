#include "Handlers.h"

#include "handle/Manager.h"
#include "sched/Task.h"
#include "sched/Thread.h"
#include "vm/Map.h"
#include "vm/MapEntry.h"

#include <stdint.h>
#include <arch.h>
#include <log.h>

using namespace sys;

using vm::kKernelVmBound;

/// Are object allocations/deallocations logged?
static bool gLogAlloc = false;
/// Are map/unmap requests logged?
static bool gLogMap = false;
/// Are map manipulations (resize, flag changes) logged?
static bool gLogChanges = false;

/**
 * Info buffer written to userspace for the "get VM region info" syscall
 */
struct sys::VmInfo {
    // base address of the region
    uintptr_t virtualBase;
    // length of the region in bytes
    uintptr_t length;

    // region flags: this is the same flags as passed to the syscall
    uintptr_t flags;
};

/**
 * Info buffer for a task's VM environment
 */
struct sys::VmTaskInfo {
    uintptr_t pagesOwned;
    uintptr_t numMappings;
};

/**
 * Flags for VM object creation
 */
enum sys::VmFlags: uintptr_t {
    None                                = 0,

    /// Force all pages in the region to be faulted in if anonymously mapped
    kNoLazyAlloc                        = (1 << 0),
    /// Use large pages, if supported
    kUseLargePages                      = (1 << 1),

    /// Allow the memory region to be read.
    kPermissionRead                     = (1 << 10),
    /// Allow the memory region to be written.
    kPermissionWrite                    = (1 << 11),
    /// Allow execution from the memory region.
    kPermissionExecute                  = (1 << 12),
    /// Optimize the mapping for MMIO use; all caching is disabled.
    kMapTypeMmio                        = (1 << 13),
    /// Allow write through caching when in MMIO mode.
    kCacheWriteThru                     = (1 << 14),
};

/**
 * Describes a request to map a particular virtual memory object into a task's address space.
 */
struct sys::VmMapRequest {
    /**
     * Start address of the range in which the kernel will search for free space to create the view
     * into the object.
     */
    uintptr_t start;
    /**
     * End of the search range, or zero if the starting address represents a fixed address at which
     * the region is to be mapped (or the call will fail). Note that this does NOT include the size
     * of the view.
     */
    uintptr_t end;

    /**
     * Desired length of the view. This may be smaller or larger than the object, but it must be a
     * multiple of the platform's page size.
     */
    size_t length;

    /**
     * Flag modifiers for the mapping. Only the access flags are considered; if any of them are
     * specified, they function as a mask on the object's permissions.
     */
    VmFlags flags = VmFlags::None;
};

/// Converts syscall flags to the mapping flags for a memory region
static vm::MappingFlags ConvertFlags(const uintptr_t flags);

/**
 * Allocates a region of virtual memory that's backed by contiguous physical memory.
 *
 * @note On 32-bit platform, this does not let us map memory above the 4G barrier, even if the
 * system supports some physical address extension mechanism! We need to address this later.
 *
 * @param physAddr Base physical address for the mapping. Must be page aligned
 * @param length Length of the region, in bytes. Must be page aligned
 * @param flags Access and cacheability flags for the region
 *
 * @return Valid handle to the VM region, or a negative error code
 */
intptr_t sys::VmAllocPhysRegion(const uintptr_t physAddr, const size_t length, const VmFlags flags) {
    rt::SharedPtr<vm::MapEntry> region = nullptr;
    const auto pageSz = arch_page_size();
    auto task = sched::Task::current();

    if(gLogAlloc) {
        log("VmAllocPhysRegion($%p, %lu, %04x)", physAddr, length, flags);
    }

    // validate the arguments
    if(physAddr % pageSz || length % pageSz) {
        return Errors::InvalidAddress;
    }

    if(!(flags & (kPermissionRead | kPermissionWrite | kPermissionExecute))) {
        // must specify a mapping permission
        return Errors::InvalidArgument;
    }

    // set up the mapping
    const auto mapFlags = ConvertFlags(flags);

    region = vm::MapEntry::makePhys(physAddr, length, mapFlags);
    if(!region) {
        return Errors::GeneralError;
    }

    // associate it with the task and return its handle
    task->addVmRegion(region);
    return static_cast<intptr_t>(region->getHandle());
}

/**
 * Allocates an anonymous memory region, backed by physical memory pages.
 *
 * @param length Size of the anonymous memory region, in bytes. Must be page aligned
 * @param flags Access and cacheability flags for the region
 *
 * @return Valid handle to the anonymous VM region, or a negative error code
 */
intptr_t sys::VmAllocAnonRegion(const uintptr_t length, const VmFlags flags) {
    rt::SharedPtr<vm::MapEntry> region = nullptr;
    const auto pageSz = arch_page_size();
    auto task = sched::Task::current();

    if(gLogAlloc) {
        log("VmAllocAnonRegion(%lu, %04x)", length, flags);
    }

    // validate arguments
    if(length % pageSz) {
        return Errors::InvalidArgument;
    }

    // set up the mapping
    const auto mapFlags = ConvertFlags(flags);

    region = vm::MapEntry::makeAnon(length, mapFlags);
    if(!region) {
        return Errors::GeneralError;
    }

    // associate it with the task and return its handle
    task->addVmRegion(region);
    return static_cast<intptr_t>(region->getHandle());
}

/**
 * Deallocates a virtual memory region. This will unmap the region from the caller (if it is
 * mapped) and if the calling task is the owner, remove the ownership reference.
 *
 * This means that it's safe to call this on a region shared with other tasks; the region will only
 * be completely deallocated when there are no more mappings to it, and the owning task has
 * deallocated it.
 */
intptr_t sys::VmDealloc(const Handle vmHandle) {
    int err;

    if(gLogAlloc) {
        log("VmDealloc($%p'h)", vmHandle);
    }

    auto task = sched::Task::current();
    if(!task || !task->vm) return Errors::GeneralError;
    auto vm = task->vm;

    // try to get a handle to the region
    auto region = handle::Manager::getVmObject(vmHandle);
    if(!region) {
        return Errors::InvalidHandle;
    }

    // unmap it from the caller task if needed
    if(vm->contains(region)) {
        err = vm->remove(region, task);
        if(err) return Errors::GeneralError;
    }

    // remove ownership, if needed
    task->removeVmRegion(region);

    // done!
    return Errors::Success;
}

/**
 * Updates the permissions flags of the VM map.
 *
 * This takes the same flags as the creation functions, but only the RWX flags are considered.
 *
 * @param vmHandle Handle to the VM object whose permissions will be updated
 * @param newFlags New permission flags to apply to VM object
 *
 * @return 0 on success, or a negative error code
 */
intptr_t sys::VmRegionUpdatePermissions(const Handle vmHandle, const VmFlags newFlags) {
    int err;

    if(gLogChanges) {
        log("VmRegionUpdatePermissions($%p'h, %04x)", vmHandle, newFlags);
    }

    // get the VM map
    auto map = handle::Manager::getVmObject(vmHandle);
    if(!map) {
        return Errors::InvalidHandle;
    }

    // convert the flags
    const auto mapFlags = ConvertFlags(newFlags);

    // set the map's flags
    // log("new flags for map %p: %04x", map, mapFlags);
    err = map->updateFlags(mapFlags);

    if(!err) {
        return Errors::Success;
    } else {
        return Errors::GeneralError;
    }
    return Errors::Success;
}

/**
 * Resizes a VM region.
 *
 * @param vmHandle Handle to the region to resize
 * @param newSize New size for the region, in bytes. Must be page aligned
 * @param flags Flags to control resize behavior. Not currently used
 *
 * @return 0 on success, or a negative error code
 */
intptr_t sys::VmRegionResize(const Handle vmHandle, const size_t newSize, const VmFlags flags) {
    int err;

    if(gLogChanges) {
        log("VmRegionResize($%p'h, %lu, %04x)", vmHandle, newSize, flags);
    }

    const auto pageSz = arch_page_size();
    if(newSize % pageSz) {
        return Errors::InvalidArgument;
    }

    // get the VM map
    auto map = handle::Manager::getVmObject(vmHandle);
    if(!map) {
        return Errors::InvalidHandle;
    }

    // resize it
    err = map->resize(newSize);

    // log("Resize status for $%08x'h (new size %u): %d", map->getHandle(), args->args[1], err);
    return (!err ? Errors::Success : Errors::GeneralError);
}



/**
 * Performs a VM mapping.
 *
 * @note We assume that the addresses and lengths are properly aligned and in userspace.
 *
 * @param region Virtual memory region to map
 * @param task Task into which the region is mapped
 * @param req Map request structure. Updated with actual mapping address
 *
 * @return 0 if successful, negative error code otherwise.
 */
static intptr_t VmRegionMapInternal(const rt::SharedPtr<vm::MapEntry> &region,
        const rt::SharedPtr<sched::Task> &task, VmMapRequest &req) {
    int err;

    auto vm = task->vm;

    // build flags mask
    auto flagMask = vm::MappingFlags::None;

    if(req.flags) {
        flagMask = ConvertFlags(req.flags);
        flagMask &= (vm::MappingFlags::PermissionsMask);
    }

    // map at a fixed address
    if(!req.end) {
        err = vm->add(region, task, req.start, flagMask, req.length);
    }
    // search for a suitable address
    else {
        err = vm->add(region, task, 0, flagMask, req.length, req.start, req.end);
    }

    // get the address at which it was actually mapped
    req.start = vm->getRegionBase(region);

    return (!err ? Errors::Success : Errors::GeneralError);
}


/**
 * Maps a VM object into a task at a fixed address.
 *
 * @param vmHandle Handle to the VM object to map
 * @param taskHandle Task to map the object in, or 0 for current task
 * @param base Virtual base address for the mapping; must be page aligned
 * @param length Length of the mapping; must be page aligned
 * @param flags Access and cacheability flags
 *
 * @return 0 on success, or negative error code
 */
intptr_t sys::VmRegionMap(const Handle vmHandle, const Handle taskHandle, const uintptr_t base, const size_t length, const VmFlags flags) {
    rt::SharedPtr<sched::Task> task;
    const auto pageSz = arch_page_size();

    if(gLogMap) {
        log("VmRegionMap($%p'h, $%p'h, $%p, %lu, %04x)", vmHandle, taskHandle, base, length, flags);
    }

    // validate some of the arguments
    if(!vmHandle || !base || !length) {
        return Errors::InvalidArgument;
    } else if(base % pageSz || length % pageSz) {
        return Errors::InvalidArgument;
    } else if((base + length) >= kKernelVmBound) {
        return Errors::InvalidAddress;
    }

    // resolve the VM object and task handle
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    auto vm = handle::Manager::getVmObject(vmHandle);
    if(!vm) {
        return Errors::InvalidHandle;
    }

    // build the request structure and perform mapping
    VmMapRequest req{base, 0, length, flags};
    return VmRegionMapInternal(vm, task, req);
}

/**
 * Long form VM object mapping routine
 *
 * @param vmHandle Handle to the VM object to map
 * @param taskHandle Task to map the object in, or 0 for current task
 * @param inReq Location of request structure in userspace. Updated on success
 * @param inReqLen Length of the request structure, in bytes
 *
 * @return 0 on success, or negative error code
 */
intptr_t sys::VmRegionMapEx(const Handle vmHandle, const Handle taskHandle, VmMapRequest *inReq,
        const size_t inReqLen) {
    rt::SharedPtr<sched::Task> task;
    const auto pageSz = arch_page_size();
    VmMapRequest req{0};

    if(gLogMap) {
        log("VmRegionMapEx($%p'h, $%p'h, %p, %d)", vmHandle, taskHandle, inReq, inReqLen);
    }

    // resolve the VM object and task handle
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    auto vm = handle::Manager::getVmObject(vmHandle);
    if(!vm) {
        return Errors::InvalidHandle;
    }

    // validate request ptr and load the structure
    if(inReqLen < sizeof(req)) {
        return Errors::InvalidArgument;
    }
    else if(!Syscall::validateUserPtr(inReq, inReqLen)) {
        return Errors::InvalidPointer;
    }

    Syscall::copyIn(inReq, inReqLen, &req, sizeof(req));

    // validate the actual request itself
    if(req.start % pageSz || req.end % pageSz || req.length % pageSz) {
        return Errors::InvalidArgument;
    } else if(!req.start || (req.start + req.length) >= kKernelVmBound ||
              (req.end + req.length) >= kKernelVmBound) {
        return Errors::InvalidAddress;
    } else if(((req.start && req.end) && req.end <= req.start)) {
        return Errors::InvalidArgument;
    }

    // perform the mapping
    return VmRegionMapInternal(vm, task, req);
}


/**
 * Unmaps the given VM region.
 */
intptr_t sys::VmRegionUnmap(const Handle vmHandle, const Handle taskHandle) {
    rt::SharedPtr<sched::Task> task;

    if(gLogMap) {
        log("VmRegionUnmap($%p'h, $%p'h)", vmHandle, taskHandle);
    }

    // get the task handle
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // get the VM map
    auto region = handle::Manager::getVmObject(vmHandle);
    if(!region) {
        return Errors::InvalidHandle;
    }

    // perform the unmapping
    int err = task->vm->remove(region, task);
    return (!err ? Errors::Success : Errors::GeneralError);
}



/**
 * Gets info for a VM region.
 *
 * @param vmHandle VM region to get info for
 * @param taskHandle Task whose virtual address space is searched (or 0 for current task)
 * @param infoPtr Location of a `VmInfo` structure in userspace
 * @param infoLen Size of the info structure, in bytes
 *
 * @return 0 on success, or a negative error code
 */
intptr_t sys::VmRegionGetInfo(const Handle vmHandle, const Handle taskHandle,
        VmInfo *infoPtr, const size_t infoLen) {
    VmInfo info{0};
    int err;
    rt::SharedPtr<sched::Task> task;

    // validate the info region buffer and size
    if(infoLen < sizeof(VmInfo)) {
        return Errors::InvalidArgument;
    }
    if(!Syscall::validateUserPtr(infoPtr, infoLen)) {
        return Errors::InvalidPointer;
    }

    // get the task handle
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // get the VM region
    auto region = handle::Manager::getVmObject(vmHandle);
    if(!region) {
        return Errors::InvalidHandle;
    }

    // ensure the task has this region mapped
    auto vm = task->vm;
    if(!vm->contains(region)) {
        return Errors::Unmapped;
    }

    // if so, fill the info
    uintptr_t base, len;
    vm::MappingFlags flags;

    err = vm->getRegionInfo(region, base, len, flags);
    if(err) {
        return Errors::GeneralError;
    }

    info.virtualBase = base;
    info.length = len;

    // convert the flags value
    uint16_t outFlags = 0;

    if(TestFlags(flags & vm::MappingFlags::Read)) {
        outFlags |= (1 << 10);
    }
    if(TestFlags(flags & vm::MappingFlags::Write)) {
        outFlags |= (1 << 11);
    }
    if(TestFlags(flags & vm::MappingFlags::Execute)) {
        outFlags |= (1 << 12);
    }
    if(TestFlags(flags & vm::MappingFlags::MMIO)) {
        outFlags |= (1 << 13);
    }

    if(region->backedByAnonymousMem()) {
        outFlags |= (1 << 7);
    }

    info.flags = outFlags;
    // log("For handle $%08x'h: base %08x len %x flags %x %x", args->args[0], base, len, (uintptr_t )outFlags, (uintptr_t) flags);

    // copy out the info buffer
    Syscall::copyOut(&info, sizeof(info), infoPtr, infoLen);

    return Errors::Success;
}

/**
 * Retrieves information about a task's VM environment.
 *
 * @param taskHandle Task to get VM info for, or 0 for current task
 * @param infoPtr Location of a `VmTaskInfo` struct in userspace
 * @param infoLen Size of the info structure, in bytes
 *
 * @return 0 on success, or a negative error code
 */
intptr_t sys::VmTaskGetInfo(const Handle taskHandle, VmTaskInfo *infoPtr, const size_t infoLen) {
    VmTaskInfo info{0};
    rt::SharedPtr<sched::Task> task;

    // validate the info region buffer and size
    if(infoLen < sizeof(VmTaskInfo)) {
        return Errors::InvalidArgument;
    }
    if(!Syscall::validateUserPtr(infoPtr, infoLen)) {
        return Errors::InvalidPointer;
    }

    // get the task handle
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // copy the information
    __atomic_load(&task->physPagesOwned, &info.pagesOwned, __ATOMIC_RELAXED);
    info.numMappings = task->vm->numMappings();

    // copy out the info buffer (TODO: use copyout)
    Syscall::copyOut(&info, sizeof(info), infoPtr, infoLen);

    return Errors::Success;
}

/**
 * Determines the virtual memory region that contains the given virtual address.
 *
 * @param taskHandle Task to look up the address in, or 0 for current task
 * @param vmAddress Virtual address to look up
 *
 * @return Handle to the VM region containing this address, 0 if not found, or negative error code
 */
intptr_t sys::VmAddrToRegion(const Handle taskHandle, const uintptr_t vmAddr) {
    rt::SharedPtr<sched::Task> task;

    // get the task handle
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // validate the virtual address
    if(vmAddr >= kKernelVmBound) {
        return Errors::InvalidAddress;
    }

    // query the task's VM object for the information
    Handle regionHandle = Handle::Invalid;
    uintptr_t offset;

    auto vm = task->vm;
    REQUIRE(vm, "failed to get vm object for task %p ($%08x'h)", static_cast<void *>(task), task->handle);

    bool found = vm->findRegion(vmAddr, regionHandle, offset);

    if(found) {
        return static_cast<intptr_t>(regionHandle);
    } else {
        return Errors::Success;
    }
}

/**
 * Converts the syscall VM flags to those required to create an anonymous mapping.
 */
static vm::MappingFlags ConvertFlags(const uintptr_t f) {
    vm::MappingFlags flags;

    if(f & kPermissionRead) {
        flags |= vm::MappingFlags::Read;
    }
    if(f & kPermissionWrite) {
        flags |= vm::MappingFlags::Write;
    }
    if(f & kPermissionExecute) {
        flags |= vm::MappingFlags::Execute;
    }
    if(f & kMapTypeMmio) {
        flags |= vm::MappingFlags::MMIO;
    }

    return flags;
}

