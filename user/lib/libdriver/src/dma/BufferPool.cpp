#include "BufferPool.h"

#include <unistd.h>
#include <sys/syscalls.h>
#include <driver/DmaBuffer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace libdriver;

/// Region of virtual memory space for buffer pools
static uintptr_t kIoBufferMappingRange[2] = {
    // start
    0x60809000000,
    // end
    0x60819000000,
};

/**
 * Allocates the buffer pool, including its virtual memory region.
 */
BufferPool::BufferPool(const size_t initial, const size_t maxSize) {
    int err;
    const auto pageSz = sysconf(_SC_PAGESIZE);

    // validate arguments
    if(!pageSz || !initial || (initial % pageSz) || !maxSize || (maxSize % pageSz)) {
        this->status = DmaBuffer::Errors::InvalidSize;
        return;
    }

    // allocate the region
    err = AllocVirtualAnonRegion(initial,
            (VM_REGION_RW | VM_REGION_WRITETHRU | VM_REGION_MMIO | VM_REGION_LOCKED),
            &this->vmHandle);
    if(err) {
        this->status = err;
        return;
    }

    // then map it into the aperture with the given maxSize
    uintptr_t base{0};
    err = MapVirtualRegionRange(this->vmHandle, kIoBufferMappingRange, maxSize, 0, &base);
    kIoBufferMappingRange[0] += maxSize;
    if(err) {
        this->status = err;
        return;
    }

    this->base = reinterpret_cast<void *>(base);

    this->maxSize = maxSize;
    this->allocatedSize = initial;

    // create the initial free item
    this->freeList.push_back({0, initial});
}

/**
 * Releases the virtual memory region. You are responsible for ensuring there are no hardware
 * devices still accessing the underlying physical memory before destroying the buffer pool.
 */
BufferPool::~BufferPool() {
    UnmapVirtualRegion(this->vmHandle);
}

/**
 * Allocates a region of the given size from the buffer pool.
 *
 * The provided buffer object will automatically return the memory back to the pool when the last
 * reference to it is dropped.
 */
int BufferPool::getBuffer(const size_t length, std::shared_ptr<Buffer> &outBuffer) {
    std::shared_ptr<Buffer> buf;

    // try to find a free block large enough that we can slice up
    for(auto it = this->freeList.begin(); it != this->freeList.end(); ++it) {
        const auto &block = *it;
        if(block.length >= length) {
            const uintptr_t offset{block.offset};

            // if the entire block is used up, remove it
            const auto remaining = block.length - length;
            if(!remaining) {
                this->freeList.erase(it);
            }
            // otherwise, update its offset and length
            else {
                it->length -= length;
                it->offset += length;
            }

            // create the buffer
            buf = std::shared_ptr<Buffer>(new Buffer(this, offset, length));
            goto success;
        }
    }

    // no sufficiently large blocks; try to grow segment (TODO)
    return -1;

success:;
    // return the buffer we allocated
    outBuffer = buf;
    return 0;
}

/**
 * Marks the given range as free again. We'll insert it into the free list.
 */
void BufferPool::freeRange(const uintptr_t offset, const size_t size) {
    // ensure the free list is ordered
    this->freeList.push_front({offset, size});
    this->freeList.sort();

    // try to defragment the freelist
    this->defragFreeList();
}

/**
 * Defragments the free list by merging adjacent blocks.
 */
void BufferPool::defragFreeList() {
    for(auto it = this->freeList.begin(); it != this->freeList.end();) {
        auto &x = *it;
        auto next = it; ++next;

        // if we're not yet at the end of the free list, see if the next entry is adjacent
        if(next != this->freeList.end()) {
            const auto thisEnd = x.offset + x.length;
            const auto nextBase = (*next).offset;

            // they are adjacent, so add the length of the next one and then remove that entry
            if(thisEnd == nextBase) {
                x.length += (*next).length;
                this->freeList.erase(next);
            }
            // they can't be merged so check the next entry
            else {
                it++;
            }
        }
        // this is the last entry and we cannot merge it
        else {
            it++;
        }
    }
}

/**
 * Allocates a buffer pool.
 */
int BufferPool::Alloc(const size_t initial, const size_t maxSize, std::shared_ptr<BufferPool> &outPtr) {
    std::shared_ptr<BufferPool> buf(new BufferPool(initial, maxSize));

    if(!buf->status) {
        outPtr = buf;
    }
    return buf->status;
}



/**
 * Initializes a sub-buffer allocated from the buffer pool.
 */
BufferPool::Buffer::Buffer(BufferPool * _Nonnull pool, const uintptr_t offset,
        const size_t length) : parent(pool), size(length), offset(offset) {
    int err;

    const size_t pageSz = sysconf(_SC_PAGESIZE);

    // ensure all pages are available
    memset(this->data(), 0, length);

    // get all the physical pages
    size_t bytesLeft = length;
    uintptr_t x = offset;

    std::vector<Extent> extents;

    while(bytesLeft) {
        // convert this offset to a virtual address and look it up
        uintptr_t physAddr{0};
        const uintptr_t virtAddr = reinterpret_cast<uintptr_t>(pool->base) + x;

        err = VirtualToPhysicalAddr(&virtAddr, 1, &physAddr);
        if(err) {
            this->status = err;
            return;
        } else if(!physAddr) {
            this->status = Errors::PhysTranslationFailed;
            return;
        }

        const auto used = pageSz - (x % pageSz);
        extents.emplace_back(physAddr, std::min(bytesLeft, used));

        // advance to the next page
        bytesLeft -= std::min(bytesLeft, used);
        x += used;
    }

    this->extents = extents;
}

/**
 * Releases the buffer pool allocation owned by this buffer back to the buffer pool.
 */
BufferPool::Buffer::~Buffer() {
    this->parent->freeRange(this->offset, this->size);
}
