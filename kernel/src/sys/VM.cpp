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
int sys::VmAlloc(const Syscall::Args *args, const uintptr_t number) {
    vm::MapEntry *region = nullptr;
    const auto pageSz = arch_page_size();

    // validate the arguments
    const auto flags = (number & 0xFFFF0000) >> 16;
    const auto physAddr = args->args[0];
    auto vmAddr = args->args[1];
    const auto length = args->args[2];

    if(!vmAddr || (vmAddr + length) >= 0xC0000000 || (length % pageSz)) {
        // must specify virtual address entirely in user region, and length must be aligned
        return Errors::InvalidAddress;
    }

    if(!(flags & (kPermissionRead | kPermissionWrite | kPermissionExecute))) {
        // must specify a mapping permission
        return Errors::InvalidArgument;
    }

    // set up the mapping
    const auto mapFlags = ConvertFlags(flags);

    region = vm::MapEntry::makePhys(physAddr, vmAddr, length, mapFlags);
    if(!region) {
        return Errors::GeneralError;
    }

    // add it to the calling task's mapping if desired
    if(!(flags & kDoNotMap)) {
        auto vm = sched::Thread::current()->task->vm;
        vm->add(region);
    } 
    // if not, associate it with the task; if nobody maps it, that will keep us from leaking it
    else {
        auto task = sched::Thread::current()->task;
        task->addOwnedVmRegion(region->retain());
    }

    // return the handle of the region
    return static_cast<int>(region->getHandle());
}

/**
 * Allocates an anonymous memory region, backed by physical memory.
 */
int sys::VmAllocAnon(const Syscall::Args *args, const uintptr_t number) {
    vm::MapEntry *region = nullptr;

    const auto pageSz = arch_page_size();

    // validate arguments
    const auto flags = (number & 0xFFFF0000) >> 16;
    auto vmAddr = args->args[0];
    const auto length = args->args[1];

    if(!vmAddr) {
        // TODO: find a place to map it
        log("kernel selection of VM map address not yet supported");
        return Errors::InvalidArgument;
    }

    if(vmAddr + length >= 0xC0000000) {
        return Errors::InvalidAddress;
    }
    if(length % pageSz) {
        return Errors::InvalidArgument;
    }

    // set up the mapping
    const auto mapFlags = ConvertFlags(flags);

    region = vm::MapEntry::makeAnon(vmAddr, length, mapFlags);
    if(!region) {
        return Errors::GeneralError;
    }

    // add it to the calling task's mapping if desired
    if(!(flags & kDoNotMap)) {
        auto vm = sched::Thread::current()->task->vm;
        vm->add(region);
    } 
    // if not, associate it with the task; if nobody maps it, that will keep us from leaking it
    else {
        auto task = sched::Thread::current()->task;
        task->addOwnedVmRegion(region->retain());
    }

    // return the handle of the region
    return static_cast<int>(region->getHandle());
}

/**
 * Updates the permissions flags of the VM map.
 *
 * This takes the same flags as the creation functions, but only the RWX flags are considered.
 */
int sys::VmRegionUpdatePermissions(const Syscall::Args *args, const uintptr_t number) {
    // get the VM map
    auto map = handle::Manager::getVmObject(static_cast<Handle>(args->args[0]));
    if(!map) {
        return Errors::InvalidHandle;
    }

    // convert the flags
    const auto flags = (number & 0xFFFF0000) >> 16;
    const auto mapFlags = ConvertFlags(flags);

    // TODO: implement
    log("new flags for map %p: %04x", map, mapFlags);
    return Errors::Success;
}

/**
 * Resizes a VM region.
 */
int sys::VmRegionResize(const Syscall::Args *args, const uintptr_t number) {
    int err;

    // get the VM map
    auto map = handle::Manager::getVmObject(static_cast<Handle>(args->args[0]));
    if(!map) {
        return Errors::InvalidHandle;
    }

    // resize it
    err = map->resize(args->args[1]);

    return (!err ? Errors::Success : Errors::GeneralError);
}

/**
 * Maps the specified VM region into the task.
 *
 * This will perform the update immediately if this is the executing task, otherwise it defers the
 * update to the next time that task is switched to.
 */
int sys::VmRegionMap(const Syscall::Args *args, const uintptr_t number) {
    sched::Task *task = nullptr;

    // get the task handle
    if(!args->args[1]) {
        task = sched::Thread::current()->task;
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
    if(base && (base + region->getLength()) >= 0xC0000000) {
        return Errors::InvalidAddress;
    }

    // perform the mapping
    int err = task->vm->add(region, base);
    return (!err ? Errors::Success : Errors::GeneralError);
}

/**
 * Unmaps the given VM region.
 */
int sys::VmRegionUnmap(const Syscall::Args *args, const uintptr_t number) {
    sched::Task *task = nullptr;

    // get the task handle
    if(!args->args[1]) {
        task = sched::Thread::current()->task;
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
    int err = task->vm->remove(region);
    return (!err ? Errors::Success : Errors::GeneralError);
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

