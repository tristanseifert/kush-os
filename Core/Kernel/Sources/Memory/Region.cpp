#include "Memory/Region.h"
#include "Memory/PhysicalAllocator.h"
#include "Memory/Pool.h"

#include "Logging/Console.h"
#include "Runtime/String.h"
#include "Vm/Map.h"
#include "Vm/ContiguousPhysRegion.h"

#include <Intrinsics.h>
#include <Platform.h>
#include <new>

using namespace Kernel::Memory;

// space in .bss segment for VM objects
static KUSH_ALIGNED(64) uint8_t gVmObjectAllocBuf[Pool::kMaxGlobalRegions][sizeof(Kernel::Vm::ContiguousPhysRegion)];
// index for the next available VM object
static size_t gVmObjectAllocNextFree{0};

/**
 * @brief Initialize a new region of physical memory.
 *
 * This sets up the associated bitmap: all pages are initially marked as available, except for
 * those used to actually store the bitmap itself.
 *
 * We'll also allocate a VM object to represent the bitmap in the kernel's address space. This is
 * not yet mapped; a later call to 
 *
 * @param pool The allocation pool this region belongs to
 * @param base Physical base address of this region
 * @param length Number of bytes in this region
 */
Region::Region(Pool *pool, const uintptr_t base, const size_t length) :
    physBase(base) {
    int err;

    REQUIRE(length, "invalid %s", "length");
    REQUIRE(pool, "invalid %s", "pool");

    // convert length into pages
    const auto pageSz = pool->allocator->getPageSize();
    this->numPages = length / pageSz;

    // calculate bitmap location and size
    const auto bitmapBytes = (this->numPages + (8 - 1)) / 8;
    const auto bitmapPages = (bitmapBytes + (pageSz - 1)) / pageSz;
    const auto allocatablePages = this->numPages - bitmapPages;

    this->bitmapPhys = base;
    this->bitmapSize = allocatablePages;
    this->bitmapReserved = (bitmapPages * pageSz);

    this->allocBasePhys = base + this->bitmapReserved;

    // initialize bitmap
    void *addr{nullptr};
    err = Platform::Memory::PhysicalMap::Add(this->bitmapPhys, this->bitmapReserved, &addr);
    REQUIRE(!err, "failed to map region bitmap: %d", err);

    this->bitmap = reinterpret_cast<uint64_t *>(addr);

    memset(this->bitmap, 0xFF, bitmapBytes);

    // allocate the VM object (TODO: check if alternate allocator is available)
    const auto idx = gVmObjectAllocNextFree++;
    auto vmRegionPtr = reinterpret_cast<Vm::ContiguousPhysRegion *>(gVmObjectAllocBuf[idx]);
    this->bitmapVm = new (vmRegionPtr) Vm::ContiguousPhysRegion(this->bitmapPhys,
            this->bitmapReserved, Vm::Mode::KernelRW);
}

/**
 * @brief Clean up the region.
 *
 * This unmaps its bitmap.
 */
Region::~Region() {
    int err = Platform::Memory::PhysicalMap::Remove(this->bitmap, this->bitmapReserved);
    if(err) {
        Console::Warning("failed to unmap region %p bitmap (%p): %d", this, this->bitmap, err);
    }
}

/**
 * @brief Attempt to allocate pages from this region.
 *
 * @param pool Pool in which this region sits
 * @param numPages Number of pages to attempt to allocate
 * @param outAddrs Buffer to receive allocated physical addresses
 *
 * @return Number of allocated pages, or a negative error code.
 *
 * @remark This function may return (and in turn, allocate) fewer pages than requested.
 */
int Region::alloc(Pool *pool, const size_t numPages, uintptr_t *outAddrs) {
    size_t satisfied{0};
    const auto pageSz = pool->allocator->getPageSize();

    // read the bitmap 64 bits at a time
    auto readPtr = this->bitmap;

    for(size_t off = 0; off < this->bitmapSize; off += 64) {
        // read this chunk; skip if zero (all pages allocated)
        auto val = *readPtr;
        if(!val) {
            readPtr++;
            continue;
        }

        // calculate the physical base address for this chunk
        const uintptr_t base = this->allocBasePhys + (off * pageSz);

        // allocate pages until this chunk is full or allocations succeeded
        do {
            // mark page as allocated
            const auto i = __builtin_ffsll(val);
            REQUIRE(i, "wtf");

            val &= ~(1ULL << (i - 1));
            *readPtr = val;

            // store its address
            *outAddrs++ = base + ((i - 1) * pageSz);
            satisfied++;

            this->numAllocated++;
        } while(val && satisfied < numPages);

        // if we've satisfied all requested allocations, return
        if(satisfied == numPages) {
            return satisfied;
        }

        // otherwise, go to the next chunk
        readPtr++;
    }

    // we were only able to partially satisfy this request
    return satisfied;
}

/**
 * @brief Free the given pages.
 *
 * @param pool Pool in which this region sits
 * @param numPages Number of pages in the specified buffer
 * @param inAddrs Physical page addresses to free
 *
 * @remark Any page addresses that do not fall within this region are ignored.
 *
 * @return Number of freed pages
 */
int Region::free(Pool *pool, const size_t numPages, const uintptr_t *inAddrs) {
    int freed{0};
    const auto pageSz = pool->allocator->getPageSize();

    for(size_t i = 0; i < numPages; i++) {
        const auto addr = *inAddrs++;
        if(this->contains(addr)) {
            const auto offset = addr - this->allocBasePhys;
            const auto page = offset / pageSz;

            this->bitmap[page / 64] |= (1ULL << (page % 64));

            freed++;
            this->numAllocated--;
        }
    }

    return freed;
}

/**
 * @brief Map the bitmap into virtual address space.
 *
 * @param base Virtual base address
 * @param map Memory map to receive the bitmap
 *
 * @return Number of bytes required for bitmap
 */
size_t Region::applyVirtualMap(uintptr_t base, Vm::Map *map) {
    // remap the region
    REQUIRE(this->bitmapVm->isOrphaned(), "cannot re-map region %p bitmap", this);
    int err = map->add(base, this->bitmapVm);
    REQUIRE(!err, "failed to map region bitmap: %d", err);

    // update pointers
    this->bitmap = reinterpret_cast<uint64_t *>(base);

    return this->bitmapReserved;
}
