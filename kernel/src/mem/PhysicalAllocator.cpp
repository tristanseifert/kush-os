#include "PhysicalAllocator.h"

#include <arch.h>
#include <platform.h>
#include <string.h>
#include <log.h>

#include <new>

using namespace mem;

static char gAllocatorBuf[sizeof(PhysicalAllocator)];
PhysicalAllocator *PhysicalAllocator::gShared = (PhysicalAllocator *) &gAllocatorBuf;

/**
 * Initializes the global physical memory allocator.
 */
void PhysicalAllocator::init() {
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

        log("region at %016llx, length %016llx, %ld pages, %ld bitmap pages", baseAddr, length, pages, bitmapPages);

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
    }
}

/**
 * Maps the usage bitmaps for each of the physical regions into virtual memory.
 *
 * This should be called immediately after virtual memory becomes available.
 */
void PhysicalAllocator::mapRegionUsageBitmaps() {
    for(size_t i = 0; i < this->numRegions; i++) {
        auto &region = this->regions[i];

        // place it in the usage bitmap region (next available)
        void *phys = region.freeMap;
        void *virt = (void *) this->nextBitmapVmAddr;

        log("mapping phys free bitmap: %p -> %p", phys, virt);

        // update for the next mapping
        //region.freeMap = (uint32_t *) virt;
        this->nextBitmapVmAddr += region.freeMapPages * arch_page_size();
    }
}



/**
 * Returns the physical address of a freshly allocated page, or 0 if we failed to allocate.
 *
 * This will try each physical allocation region in turn.
 */
size_t PhysicalAllocator::allocPage() {
    int err;
    size_t idx = 0;

    const auto pageSz = arch_page_size();

    // try each region
    for(size_t i = 0; i < this->numRegions; i++) {
        auto &region = this->regions[i];

        // allocate one page from the region
        err = region.alloc(&idx);
        if(!err) {
            // if success, convert to physical address
            return region.base + ((idx + region.freeMapPages) * pageSz);
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
int PhysicalAllocator::Region::alloc(size_t *freeIdx) {
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
void PhysicalAllocator::freePage(const size_t physAddr) {
    const auto pageSz = arch_page_size();

    // find the corresponding region
    for(size_t i = 0; i < this->numRegions; i++) {
        auto &region = this->regions[i];
        if(!region.contains(physAddr)) continue;

        // calculate page index and free it
        const auto index = ((physAddr - region.base) / pageSz) - region.freeMapPages;
        region.free(index);

        return;
    }

    panic("no region for phys addr $%p", (void *) physAddr);
}

/**
 * Frees the page with the given index (its address being base + index * page size)
 */
void PhysicalAllocator::Region::free(const size_t idx) {
    // ensure index in bounds
    if(idx >= this->totalPages) {
        panic("page %lu out of bounds (max %lu)", idx, this->totalPages);
    }

    // set the bit
    const auto index = idx / 32;
    const auto bit = idx % 32;

    this->freeMap[index] |= (1 << bit);
}
