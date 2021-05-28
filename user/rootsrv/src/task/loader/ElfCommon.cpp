#include "ElfCommon.h"
#include "../Task.h"
#include "StringHelpers.h"

#include <fmt/format.h>
#include <sys/elf.h>

#include <log.h>

#include <unistd.h>
#include <cstring>
#include <system_error>

using namespace task::loader;

const uintptr_t ElfCommon::kTempMappingRange[2] = {
    // start
    0x10000000000,
    // end
    0x20000000000,
};

/**
 * Sets up the stack memory pages.
 */
void ElfCommon::setUpStack(const std::shared_ptr<Task> &task, const uintptr_t infoStructAddr) {
    int err;
    uintptr_t vmHandle;
    uintptr_t base;

    // TODO: get from sysconf
    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to determine page size");
    }

    // allocate the anonymous region
    size_t stackSize = this->getDefaultStackSize();
    stackSize = ((stackSize + pageSz - 1) / pageSz) * pageSz;

    uintptr_t stackAddr = this->getDefaultStackAddr();

    err = AllocVirtualAnonRegion(stackSize, VM_REGION_RW | VM_REGION_FORCE_ALLOC, &vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "AllocVirtualAnonRegion");
    }

    // map it in the current process address space and set up the stack frame
    err = MapVirtualRegionRange(vmHandle, kTempMappingRange, stackSize, 0, &base);

    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionRange");
    }

    auto lastPage = reinterpret_cast<std::byte *>(base + stackSize - pageSz);
    memset(lastPage, 0, pageSz);

    // build the stack frame
    this->stackBottom = (stackAddr + stackSize) - sizeof(uintptr_t);

    auto argPtr = reinterpret_cast<uintptr_t *>(base + stackSize);
    argPtr[-1] = infoStructAddr;

    // place the mapping into the task
    err = MapVirtualRegionRemote(task->getHandle(), vmHandle, stackAddr, stackSize, 0);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionRemote");
    }

    // then unmap it from our task
    err = UnmapVirtualRegion(vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "UnmapVirtualRegion");
    }
}
