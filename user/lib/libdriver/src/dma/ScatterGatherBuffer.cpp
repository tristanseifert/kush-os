#include "ScatterGatherBuffer.h"

#include <unistd.h>
#include <sys/syscalls.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <system_error>

using namespace libdriver;

/// Region of virtual memory space for scatter/gather buffers
static uintptr_t kMappingRange[2] = {
    // start
    0x69800000000,
    // end
    0x69F00000000,
};

/**
 * Initializes the scatter/gather buffer.
 *
 * Note that the entirety of the buffer will be allocated and locked in memory for as long as the
 * object exists.
 */
ScatterGatherBuffer::ScatterGatherBuffer(const size_t _size) : size(_size) {
    int err;

    // round the size up
    const size_t pageSz = sysconf(_SC_PAGESIZE);
    assert(pageSz);

    const size_t size = ((_size + pageSz - 1) / pageSz) * pageSz;

    // allocate a region and map it
    err = AllocVirtualAnonRegion(size,
            (VM_REGION_RW | VM_REGION_WRITETHRU | VM_REGION_MMIO | VM_REGION_LOCKED |
             VM_REGION_FORCE_ALLOC),
            &this->vmHandle);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "AllocVirtualAnonRegion");
    }

    uintptr_t base{0};
    err = MapVirtualRegionRange(this->vmHandle, kMappingRange, size, 0, &base);
    kMappingRange[0] += size; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegion");
    }

#ifndef NDEBUG
    // Fill the buffer with a test pattern for debugging
    memset(reinterpret_cast<void *>(base), 0xAA, size);
    memset(reinterpret_cast<void *>(base), 0xFF, _size);
#endif

    // build up the extent map for each page
    const size_t numPages = size / pageSz;
    this->extents.reserve(numPages);

    size_t bytesLeft = _size;
    for(size_t i = 0; i < numPages; i++) {
        uintptr_t virtAddr{base + (i * pageSz)};
        uintptr_t physAddr{0};

        err = VirtualToPhysicalAddr(&virtAddr, 1, &physAddr);
        if(err) {
            throw std::system_error(err, std::generic_category(), "VirtualToPhysicalAddr");
        } else if(!physAddr) {
            throw std::runtime_error("failed to get physical address for page");
        }

        this->extents.emplace_back(physAddr, std::min(bytesLeft, pageSz));
        bytesLeft -= pageSz;
    }
}

/**
 * Releases the allocated memory for the buffer.
 */
ScatterGatherBuffer::~ScatterGatherBuffer() {
    // destroy the region
    if(this->vmHandle) {
        UnmapVirtualRegion(this->vmHandle);
    }
}

/**
 * Allocates a new scatter/gather buffer
 */
std::shared_ptr<ScatterGatherBuffer> ScatterGatherBuffer::Alloc(const size_t size) {
    auto buf = std::make_shared<ScatterGatherBuffer>(size);
    return buf;
}

