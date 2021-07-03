#include "ScatterGatherBuffer.h"

#include <unistd.h>
#include <sys/syscalls.h>

#include <algorithm>
#include <cassert>

using namespace libdriver;

/// Region of virtual memory space for scatter/gather buffers
static uintptr_t kMappingRange[2] = {
    // start
    0x60800000000,
    // end
    0x60808000000,
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
        this->err = err;
        return;
    }

    uintptr_t base{0};
    err = MapVirtualRegionRange(this->vmHandle, kMappingRange, size, 0, &base);
    kMappingRange[0] += size; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        this->err = err;
        return;
    }

#ifndef NDEBUG
    // Fill the buffer with a test pattern for debugging
    memset(reinterpret_cast<void *>(base), 0xAA, size);
    memset(reinterpret_cast<void *>(base), 0xFF, _size);
#endif

    this->base = reinterpret_cast<void *>(base);

    // build up the extent map for each page
    const size_t numPages = size / pageSz;
    this->extents.reserve(numPages);

    size_t bytesLeft = _size;
    for(size_t i = 0; i < numPages; i++) {
        uintptr_t virtAddr{base + (i * pageSz)};
        uintptr_t physAddr{0};

        err = VirtualToPhysicalAddr(&virtAddr, 1, &physAddr);
        if(err) {
            this->err = err;
            return;
        } else if(!physAddr) {
            this->err = Errors::PhysTranslationFailed;
            return;
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
int ScatterGatherBuffer::Alloc(const size_t size, std::shared_ptr<ScatterGatherBuffer> &outPtr) {
    std::shared_ptr<ScatterGatherBuffer> buf(new ScatterGatherBuffer(size));

    if(!buf->err) {
        outPtr = buf;
    }
    return buf->err;
}

