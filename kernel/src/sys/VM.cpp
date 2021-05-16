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

/// Cutoff for the user/kernel boundary
#if defined(__i386__)
constexpr static const uintptr_t kKernelVmBound = 0xC0000000;
#elif defined(__amd64__)
constexpr static const uintptr_t kKernelVmBound = (1ULL << 63);
#endif

/**
 * Info buffer written to userspace for the "get VM region info" syscall
 */
struct VmInfo {
    // base address of the region
    uintptr_t virtualBase;
    // length of the region in bytes
    uintptr_t length;

    uint16_t reserved;
    // region flags: this is the same flags as passed to the syscall
    uint16_t flags;
};

/**
 * Info buffer for a task's VM environment
 */
struct VmTaskInfo {
    uintptr_t pagesOwned;
    uintptr_t numMappings;
};

/**
 * Flags for VM object creation
 */
enum Flags: uint16_t {
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

    /// Do not add the allocated region to the calling task's VM map.
    kDoNotMap                           = (1 << 15),
};

/// Converts syscall flags to the mapping flags for a memory region
static vm::MappingFlags ConvertFlags(const uintptr_t flags);

/**
 * Allocates a region of virtual memory that's backed by contiguous physical memory.
 *
 * @note On 32-bit platform, this does not let us map memory above the 4G barrier, even if the
 * system supports some physical address extension mechanism! We need to address this later.
 */
intptr_t sys::VmAlloc(const Syscall::Args *args, const uintptr_t number) {
    int err;
    rt::SharedPtr<vm::MapEntry> region = nullptr;
    const auto pageSz = arch_page_size();
    auto task = sched::Task::current();

    // validate the arguments
    const auto flags = (number & 0xFFFF0000) >> 16;
    const auto physAddr = args->args[0];
    auto vmAddr = args->args[1];
    const auto length = args->args[2];

    if((vmAddr + length) >= kKernelVmBound || (length % pageSz)) {
        // must specify virtual address entirely in user region, and length must be aligned
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

    // add it to the calling task's mapping if desired
    if(!(flags & kDoNotMap)) {
        auto vm = task->vm;
        err = vm->add(region, task, vmAddr);

        if(err) {
            return Errors::GeneralError;
        }
    } 
    // if not, associate it with the task; if nobody maps it, that will keep us from leaking it
    else {
        task->addOwnedVmRegion(region);
    }

    // return the handle of the region
    return static_cast<intptr_t>(region->getHandle());
}

/**
 * Allocates an anonymous memory region, backed by physical memory.
 */
intptr_t sys::VmAllocAnon(const Syscall::Args *args, const uintptr_t number) {
    rt::SharedPtr<vm::MapEntry> region = nullptr;
    int err;
    const auto pageSz = arch_page_size();
    auto task = sched::Task::current();

    // validate arguments
    const auto flags = (number & 0xFFFF0000) >> 16;
    auto vmAddr = args->args[0];
    const auto length = args->args[1];

    if(vmAddr + length >= kKernelVmBound) {
        return Errors::InvalidAddress;
    }
    if(length % pageSz) {
        return Errors::InvalidArgument;
    }

    // set up the mapping
    const auto mapFlags = ConvertFlags(flags);

    region = vm::MapEntry::makeAnon(length, mapFlags);
    if(!region) {
        return Errors::GeneralError;
    }

    // add it to the calling task's mapping if desired
    if(!(flags & kDoNotMap)) {
        auto vm = task->vm;
        err = vm->add(region, task, vmAddr);

        if(err) {
            return Errors::GeneralError;
        }
    } 
    // if not, associate it with the task; if nobody maps it, that will keep us from leaking it
    else {
        task->addOwnedVmRegion(region);
    }

    // return the handle of the region
    return static_cast<intptr_t>(region->getHandle());
}

/**
 * Updates the permissions flags of the VM map.
 *
 * This takes the same flags as the creation functions, but only the RWX flags are considered.
 */
intptr_t sys::VmRegionUpdatePermissions(const Syscall::Args *args, const uintptr_t number) {
    int err;
    // get the VM map
    auto map = handle::Manager::getVmObject(static_cast<Handle>(args->args[0]));
    if(!map) {
        return Errors::InvalidHandle;
    }

    // convert the flags
    const auto flags = (number & 0xFFFF0000) >> 16;
    const auto mapFlags = ConvertFlags(flags);

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
 */
intptr_t sys::VmRegionResize(const Syscall::Args *args, const uintptr_t number) {
    int err;

    // get the VM map
    auto map = handle::Manager::getVmObject(static_cast<Handle>(args->args[0]));
    if(!map) {
        return Errors::InvalidHandle;
    }

    // resize it
    err = map->resize(args->args[1]);

    // log("Resize status for $%08x'h (new size %u): %d", map->getHandle(), args->args[1], err);
    return (!err ? Errors::Success : Errors::GeneralError);
}

/**
 * Maps the specified VM region into the task.
 *
 * This will perform the update immediately if this is the executing task, otherwise it defers the
 * update to the next time that task is switched to.
 */
intptr_t sys::VmRegionMap(const Syscall::Args *args, const uintptr_t number) {
    int err;
    rt::SharedPtr<sched::Task> task = nullptr;

    // get the task handle
    if(!args->args[1]) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[1]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // get the VM map
    auto region = handle::Manager::getVmObject(static_cast<Handle>(args->args[0]));
    if(!region) {
        return Errors::InvalidHandle;
    }

    // validate the base address
    const auto base = args->args[2];
    if(base && (base + region->getLength()) >= kKernelVmBound) {
        return Errors::InvalidAddress;
    }

    // perform the mapping, applying the flags mask if needed
    const auto flags = (number & 0xFFFF0000) >> 16;

    if(flags) {
        auto flagMask = ConvertFlags(flags);
        flagMask &= (vm::MappingFlags::PermissionsMask);

        err = task->vm->add(region, task, base, flagMask);
    } else {
        err = task->vm->add(region, task, base);
    }

    return (!err ? Errors::Success : Errors::GeneralError);
}

/**
 * Unmaps the given VM region.
 */
intptr_t sys::VmRegionUnmap(const Syscall::Args *args, const uintptr_t number) {
    rt::SharedPtr<sched::Task> task = nullptr;

    // get the task handle
    if(!args->args[1]) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[1]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // get the VM map
    auto region = handle::Manager::getVmObject(static_cast<Handle>(args->args[0]));
    if(!region) {
        return Errors::InvalidHandle;
    }

    // perform the unmapping
    int err = task->vm->remove(region, task);
    return (!err ? Errors::Success : Errors::GeneralError);
}

/**
 * Gets info for a VM region.
 */
intptr_t sys::VmRegionGetInfo(const Syscall::Args *args, const uintptr_t number) {
    int err;
    rt::SharedPtr<sched::Task> task = nullptr;

    // validate the info region buffer and size
    auto infoPtr = reinterpret_cast<VmInfo *>(args->args[2]);
    const auto infoLen = args->args[3];

    if(infoLen != sizeof(VmInfo)) {
        return Errors::InvalidArgument;
    }
    if(!Syscall::validateUserPtr(infoPtr, infoLen)) {
        return Errors::InvalidPointer;
    }
    memset(infoPtr, 0, sizeof(VmInfo));

    // get the task handle
    if(!args->args[1]) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[1]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // get the VM region
    auto region = handle::Manager::getVmObject(static_cast<Handle>(args->args[0]));
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

    infoPtr->virtualBase = base;
    infoPtr->length = len;

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

    infoPtr->flags = outFlags;
    // log("For handle $%08x'h: base %08x len %x flags %x %x", args->args[0], base, len, (uintptr_t )outFlags, (uintptr_t) flags);

    return Errors::Success;
}

/**
 * Retrieves information about a task's VM environment.
 */
intptr_t sys::VmTaskGetInfo(const Syscall::Args *args, const uintptr_t number) {
    rt::SharedPtr<sched::Task> task = nullptr;

    // validate the info region buffer and size
    auto infoPtr = reinterpret_cast<VmTaskInfo *>(args->args[1]);
    const auto infoLen = args->args[2];

    if(infoLen != sizeof(VmTaskInfo)) {
        return Errors::InvalidArgument;
    }
    if(!Syscall::validateUserPtr(infoPtr, infoLen)) {
        return Errors::InvalidPointer;
    }

    // get the task handle
    if(!args->args[0]) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[0]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // copy the information
    __atomic_load(&task->physPagesOwned, &infoPtr->pagesOwned, __ATOMIC_RELAXED);
    infoPtr->numMappings = task->vm->numMappings();

    return Errors::Success;
}

/**
 * Determines the virtual memory region that contains the given virtual address.
 */
intptr_t sys::VmAddrToRegion(const Syscall::Args *args, const uintptr_t number) {
    rt::SharedPtr<sched::Task> task = nullptr;

    // get the task handle
    if(!args->args[0]) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[0]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // validate the virtual address
    const auto vmAddr = args->args[1];

    if(vmAddr >= kKernelVmBound) {
        return Errors::InvalidAddress;
    }

    // query the task's VM object for the information
    Handle regionHandle;
    uintptr_t offset;

    auto vm = task->vm;
    REQUIRE(vm, "failed to get vm object for task %p ($%08x'h)", static_cast<void *>(task), task->handle);

    bool found = vm->findRegion(vmAddr, regionHandle, offset);

    if(found) {
        return static_cast<intptr_t>(regionHandle);
    } else {
        return 0; // = Errors::Success
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

