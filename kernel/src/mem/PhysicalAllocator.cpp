#include "PhysicalAllocator.h"

#include <arch.h>
#include <platform.h>
#include <string.h>
#include <log.h>

#include "sched/Task.h"

#include "vm/Mapper.h"
#include "vm/Map.h"

#include <new>

// log found physical memory regions
#define LOG_REGIONS             1
// log the virtual addresses of bitmap buffers
#define LOG_BITMAP_MAP          0
// log allocations and deallocations
#define LOG_ALLOC               0

using namespace mem;

static char gAllocatorBuf[sizeof(PhysicalAllocator)] __attribute__((aligned(64)));
PhysicalAllocator *PhysicalAllocator::gShared = nullptr;

/**
 * Initializes the global physical memory allocator.
 */
void PhysicalAllocator::init() {
    gShared = (PhysicalAllocator *) &gAllocatorBuf;
    new(gShared) PhysicalAllocator();
}

/**
 * Initializes the physical allocator. This creates free bitmaps for each of the available,
 * allocatable RAM regions.
 */
PhysicalAllocator::PhysicalAllocator() {
    int err;

    const auto numEntries = platform_phys_num_regions();
    if(numEntries <= 0 || numEntries >= (int) kMaxRegions) {
        panic("invalid number of physical regions: %d", numEntries);
    }

    for(int i = 0; i < numEntries; i++) {
        uint64_t baseAddr, length;
        err = platform_phys_get_info(i, &baseAddr, &length);
        REQUIRE(!err, "failed to get info for physical region %d: %d", i, err);

        // calculate the number of pages and size the alloc map as such
        const auto pageSz = arch_page_size();
        const size_t pages = length / pageSz;
        const size_t bitmapPages = ((pages / 8) + pageSz - 1) / pageSz;

#if LOG_REGIONS
        log("region at %016llx, length %016llx, %zd pages, %zd bitmap pages", baseAddr, length, pages, bitmapPages);
#endif

        // build the region struct. note we cap pages to be a multiple of 32
        Region r(baseAddr, length);
        r.totalPages = (pages - bitmapPages) & ~0x1F;
        r.freeMapPages = bitmapPages;

        // the address is physical; it gets translated later.
        r.freeMap = (uint32_t *) baseAddr;

        // XXX: this will crash if this region is outsie the first 4M for x86
        memset(r.freeMap, 0xFF, bitmapPages * pageSz);

        // store it
        this->regions[this->numRegions++] = r;
        this->totalPages += pages;
    }
}

/**
 * Maps the usage bitmaps for each of the physical regions into virtual memory.
 *
 * This should be called immediately after virtual memory becomes available.
 */
void PhysicalAllocator::mapRegionUsageBitmaps() {
    int err;

    const auto pageSz = arch_page_size();
    auto m = vm::Map::kern();

    for(size_t i = 0; i < this->numRegions; i++) {
        auto &region = this->regions[i];

        // place it in the usage bitmap region (next available)
        void *phys = region.freeMap;
        const auto numBytes = region.freeMapPages * pageSz;

#if LOG_BITMAP_MAP
        log("mapping phys free bitmap: %p -> %08lx", phys, this->nextBitmapVmAddr);
#endif

        err = m->add((uint64_t) phys, numBytes, this->nextBitmapVmAddr, vm::MapMode::kKernelRW);
        REQUIRE(!err, "failed to map phys free bitmap: %d", err);

        // update for the next mapping
        region.freeMap = (uint32_t *) this->nextBitmapVmAddr;
        this->nextBitmapVmAddr += numBytes;
    }
}



/**
 * Returns the physical address of a freshly allocated page, or 0 if we failed to allocate.
 *
 * This will try each physical allocation region in turn.
 */
uint64_t PhysicalAllocator::allocPage() {
    int err;
    size_t idx = 0;

    const auto pageSz = arch_page_size();

    SPIN_LOCK_GUARD(this->bitmapLock);

    // try each region
    for(size_t i = 0; i < this->numRegions; i++) {
        auto &region = this->regions[i];

        // allocate one page from the region
        err = region.alloc(&idx);
        if(!err) {
            // if success, convert to physical address
            auto addr = region.base + ((idx + region.freeMapPages) * pageSz);
#if LOG_ALLOC
            log("Allocated phys %016llx", addr);
#endif
            __atomic_add_fetch(&this->allocatedPages, 1, __ATOMIC_RELAXED);
            return addr;
        }
    }

    // if we get here, no regions have available memory
    return 0;
}

/**
 * Gets the index of the next free page in the region, if there is one.
 *
 * @return -1 if error, 0 if successful.
 */
int PhysicalAllocator::Region::alloc(uintptr_t *freeIdx) {
    // start the search at the next free page
    bool first = true;
    size_t start = this->nextFree / 32;

search:;
    // search for free pages, 32 at a time
    const auto elements = (this->totalPages + 32 - 1) / 32;
    for(size_t i = start; i < elements; i++) {
        // if no free pages in this element, check next
        if(!this->freeMap[i]) continue;

        // get index of the first free (set) bit. can return 0 if no bits set but we guard above
        const size_t allocBit = __builtin_ffs(this->freeMap[i]) - 1;

        // mark it as allocated and get the index
        this->freeMap[i] &= ~(1 << allocBit);

        *freeIdx = (i * 32) + allocBit;
        this->nextFree = i;

        // log("allocated at %p (%lu %lu)", &this->freeMap[i], i, allocBit);

        return 0;
    }

    // repeat search at the start of the region if nothing found
    if(first) {
        start = 0;
        first = false;
        goto search;
    }

    // if we get here, we failed to allocate a page
    return -1;
}

/**
 * Frees a previously allocated physical page.
 */
void PhysicalAllocator::freePage(const uint64_t physAddr) {
    const auto pageSz = arch_page_size();

    SPIN_LOCK_GUARD(this->bitmapLock);

    // find the corresponding region
    for(size_t i = 0; i < this->numRegions; i++) {
        auto &region = this->regions[i];
        if(!region.contains(physAddr)) continue;

        // calculate page index and free it
        const auto index = ((physAddr - region.base) / pageSz) - region.freeMapPages;
        region.free(index);

#if LOG_ALLOC
            log("Freeing phys %016llx", physAddr);
#endif

        __atomic_sub_fetch(&this->allocatedPages, 1, __ATOMIC_RELAXED);
        return;
    }

    panic("no region for phys addr $%p", (void *) physAddr);
}

/**
 * Reserves a given memory page. This is the same as marking it as allocated, with the
 * understanding that we'll never go and free it later.
 *
 * This is used for stuff like ACPI tables or other data structures the system has loaded into
 * main memory, and we need to keep around for some time. We should strive to copy as much of these
 * data into kernel-private buffers but for some things that is simply not feasible because system
 * firmware expects buffers at their existing locations.
 */
void PhysicalAllocator::reservePage(const uint64_t physAddr) {
    const auto pageSz = arch_page_size();

    SPIN_LOCK_GUARD(this->bitmapLock);

    // find the corresponding region
    for(size_t i = 0; i < this->numRegions; i++) {
        auto &region = this->regions[i];
        if(!region.contains(physAddr)) continue;

        // calculate page index and free it
        const auto index = ((physAddr - region.base) / pageSz) - region.freeMapPages;
        region.reserve(index);

        return;
    }

    // if we get here, the page isn't inside the phys pool so we can ignore it
}

/**
 * Frees the page with the given index (its address being base + index * page size)
 */
void PhysicalAllocator::Region::free(const uintptr_t idx) {
    // ensure index in bounds
    if(idx >= this->totalPages) {
        panic("page %zu out of bounds (max %zu)", idx, this->totalPages);
    }

    // set the bit
    const auto index = idx / 32;
    const auto bit = idx % 32;

    this->freeMap[index] |= (1 << bit);
}
