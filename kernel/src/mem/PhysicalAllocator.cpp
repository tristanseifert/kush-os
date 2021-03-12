#include "PhysicalAllocator.h"
#include "PhysRegion.h"

#include <arch.h>
#include <platform.h>
#include <string.h>
#include <log.h>

#include "sched/Task.h"

#include "vm/Mapper.h"
#include "vm/Map.h"

#include <new>

using namespace mem;

bool PhysicalAllocator::gLogSkipped = false;
bool PhysicalAllocator::gLogRegions = true;
bool PhysicalAllocator::gLogAlloc = true;

// static memory for phys regions
static uint8_t gPhysRegionBuf[sizeof(PhysRegion) * PhysicalAllocator::kMaxRegions] 
    __attribute__((aligned(64)));

// static memory for physical allocator
static uint8_t gAllocatorBuf[sizeof(PhysicalAllocator)] __attribute__((aligned(64)));
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

    // set up some state
    for(size_t i = 0; i < kMaxRegions; i++) {
        this->regions[i] = nullptr;
    }

    // iterate over each of the entries
    const auto numEntries = platform_phys_num_regions();

    for(int i = 0; i < numEntries; i++) {
        // get info on this region
        uint64_t baseAddr, length;
        err = platform_phys_get_info(i, &baseAddr, &length);
        REQUIRE(!err, "failed to get info for physical region %d: %d", i, err);

        // skip if not suitable
        if(!PhysRegion::CanAllocate(baseAddr, length)) {
            if(gLogSkipped) {
                log("skipping region at $%p (%u bytes)", baseAddr, length);
            }
            continue;
        }

        // ensure we've space for this region
        REQUIRE(this->numRegions < kMaxRegions, "too many phys regions");

        // allocate the phys region object
        void *regionPtr = gPhysRegionBuf + (this->numRegions * sizeof(PhysRegion));
        auto region = new(regionPtr) PhysRegion(baseAddr, length);

        this->regions[this->numRegions++] = region;
        //this->totalPages += pages;
    }

    REQUIRE(this->numRegions, "failed to allocate phys regions");
}

/**
 * Maps the usage bitmaps for each of the physical regions into virtual memory.
 *
 * This should be called immediately after virtual memory becomes available.
 */
void PhysicalAllocator::mapRegionUsageBitmaps() {
    // before we begin, acquire a bunch of pages to satisfy allocations for paging structures later
    for(size_t i = 0; i < kNumVmPagesCached; i++) {
        auto page = this->allocPage();
        REQUIRE(page, "VM fixup pre-allocation failed (%u)", i);

        this->vmPageCache[i] = page;
    }

    this->vmRelocating = true;

    // perform the relocations
    for(size_t i = 0; i < kMaxRegions; i++) {
        if(!this->regions[i]) break;

        const auto base = kRegionInfoBase + (i * kRegionInfoEntryLength);
        this->regions[i]->vmAvailable(base, kRegionInfoEntryLength);
    }

    // release any pages we didn't end up using
    size_t leftover = 0;
    for(size_t i = 0; i < kNumVmPagesCached; i++) {
        if(!this->vmPageCache[i]) continue;

        this->freePage(this->vmPageCache[i]);
        this->vmPageCache[i] = 0;

        leftover++;
    }

    if(gLogRegions) {
        log("%u pages of VM init cache unused", leftover);
    }

    this->vmRelocating = false;
}



/**
 * Attempts to satisfy an allocation request for contiguous physical memory.
 *
 * @param numPages Total number of contiguous pages to allocate
 * @param flags Flags to change the allocator behavior
 * @return Physical page address, or 0 if no page is available
 */
uint64_t PhysicalAllocator::alloc(const size_t numPages, const PhysFlags flags) {
    // perform normal allocation flow
    if(!this->vmRelocating) [[likely]] {
        // XXX: this is stupid
        for(size_t i = 0; i < this->numRegions; i++) {
            // if in VM mode, but this region hasn't been updated yet, bail
            if(this->vmRelocating) continue;

            // try allocating from this region
            const auto page = this->regions[i]->alloc(numPages);
            if(page) {
                return page;
            }
        }
    }

    /*
     * During the VM fixup stage, we may need to allocate memory for stuff like paging structures,
     * but the physical allocator is in an inconsistent state. So, because we can't call into any
     * regions, try to satisfy the request from a small cache we populate before the process
     * begins.
     */
    for(size_t i = 0; i < kNumVmPagesCached; i++) {
        auto entry = this->vmPageCache[i];
        if(!entry) continue;

        this->vmPageCache[i] = 0;
        return entry;
    }

    // if we get here, no regions have available memory
    return 0;
}


/**
 * Frees a previously allocated set of contiguous physical pages.
 */
void PhysicalAllocator::free(const size_t numPages, const uint64_t physAddr) {
    // test which region it came from
    for(size_t i = 0; i < this->numRegions; i++) {
        auto region = this->regions[i];

        if(!region->checkAddress(physAddr)) {
            continue;
        }

        // found the region, free it
        return;
    }

    // failed to free this phys address range
    panic("failed to free phys range $%p (%u pages)", physAddr, numPages);
}

