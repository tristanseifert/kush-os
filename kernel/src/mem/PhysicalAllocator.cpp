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

bool PhysicalAllocator::gLogSkipped     = false;
bool PhysicalAllocator::gLogRegions     = false;

// static memory for phys regions
static PhysRegion gPhysRegionBuf[PhysicalAllocator::kMaxRegions];

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

    // create physical region structs for all allocatable memory
    const auto numEntries = platform_phys_num_regions();

    for(int i = 0; i < numEntries; i++) {
        uint64_t baseAddr, length;
        err = platform_phys_get_info(i, &baseAddr, &length);
        REQUIRE(!err, "failed to get info for physical region %d: %d", i, err);

        if(gLogRegions) {
            log("Region %lu: base $%p length $%x", i, baseAddr, length);
        }

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
        void *regionPtr = &gPhysRegionBuf[this->numRegions];
        auto region = new(regionPtr) PhysRegion(baseAddr, length);

        this->regions[this->numRegions++] = region;
    }

    REQUIRE(this->numRegions, "failed to allocate phys regions");
}

/**
 * Notifies all allocated physical memory regions that the virtual memory system has been fully set
 * up, and any additional memory (which wasn't mapped during bootstrap) can now be managed.
 */
void PhysicalAllocator::notifyRegionsVm() {
    for(size_t i = 0; i < kMaxRegions; i++) {
        if(!this->regions[i]) break;

        this->regions[i]->initNextIfNeeded();
    }
}



/**
 * Attempts to allocate a single page of physical memory.
 *
 * @return Physical address of an allocated page, or 0 if allocation failed
 */
uint64_t PhysicalAllocator::allocPage(const PhysFlags flags) {
    // TODO: check per-CPU free list cache

    // ask each region
    for(size_t i = 0; i < this->numRegions; i++) {
        auto region = this->regions[i];

        while(region) {
            // try to allocate a page
            if(auto page = region->alloc()) {
                __atomic_fetch_add(&this->allocatedPages, 1, __ATOMIC_RELAXED);
                return page;
            }

            // test additional chunks in this region
            region = region->next;
        }
    }

    // no region had available memory
    return 0;
}

/**
 * Frees a single page.
 */
void PhysicalAllocator::freePage(const uint64_t physAddr) {
    // test which region it came from
    for(size_t i = 0; i < this->numRegions; i++) {
        auto region = this->regions[i];

        while(region) {
            // the region contains this address! free it
            if(region->checkAddress(physAddr)) {
                region->free(physAddr);
                __atomic_fetch_sub(&this->allocatedPages, 1, __ATOMIC_RELAXED);
                return;
            }

            // test additional chunks in this region
            region = region->next;
        }
    }

    // failed to free this page
    panic("failed to free phys page $%p", physAddr);
}



/**
 * Iterates over all physical regions and sums up their allocatable byte count.
 */
size_t PhysicalAllocator::getTotalPages() {
    uint64_t bytes{0};

    for(size_t i = 0; i < kMaxRegions; i++) {
        auto region = gShared->regions[i];
        if(!region) continue;

        bytes += region->getAvailableBytes();
    }

    // convert the bytes to pages
    return bytes / arch_page_size();
}
