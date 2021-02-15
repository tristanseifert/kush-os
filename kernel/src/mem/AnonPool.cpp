#include "AnonPool.h"
#include "PhysicalAllocator.h"

#include "vm/Map.h"

#include <arch.h>
#include <log.h>
#include <string.h>
#include <new>

/// Log initialization for the memory pool
#define LOG_INIT                        0

using namespace mem;

AnonPool *AnonPool::gShared = nullptr;

/**
 * Sets up the anon pool.
 */
void AnonPool::init() {
    int err;
    auto map = vm::Map::kern();

    // figure out how many pages we need
    const auto pageSz = arch_page_size();
    const auto numPages = (sizeof(AnonPool) + pageSz - 1) / pageSz;

    // allocate and map them at the beginning of the anon pool memory
    for(size_t i = 0; i < numPages; i++) {
        auto page = mem::PhysicalAllocator::alloc();
        REQUIRE(page, "failed to get phys page for anon pool struct");

        const auto virtAddr = kBaseAddr + (i * pageSz);
        err = map->add(page, pageSz, virtAddr, vm::MapMode::kKernelRW | vm::MapMode::GLOBAL);
        REQUIRE(!err, "failed to map anon pool: %d", err);
    }

    // allocate the pool there
    gShared = (AnonPool *) kBaseAddr;
    new(gShared) AnonPool(kBaseAddr + (numPages * pageSz), (kRegionLength / pageSz) - numPages);
}

/**
 * Initializes the anon pool. This just sets up the housekeeping for deciding what virtual
 * addresses are available.
 */
AnonPool::AnonPool(const uintptr_t allocBase, const size_t _numPages) : virtBase(allocBase) {
    int err;
    auto map = vm::Map::kern();

    // align to 32 page multiple
    this->totalPages = _numPages & ~0x1F;

    // allocate the bitmap
    const auto pageSz = arch_page_size();
    const auto bitmapBytes = this->totalPages / 32;
    const auto bitmapPages = (bitmapBytes + pageSz - 1) / pageSz;

#if LOG_INIT
    log("%p base alloc %08lx (%lu pages)", this, this->virtBase, this->totalPages);
    log("alloc bitmap requires %lu bytes (%lu pages)", bitmapBytes, bitmapPages);
#endif

    // allocate the bitmap and set it
    for(size_t i = 0; i < bitmapPages; i++) {
        auto page = mem::PhysicalAllocator::alloc();
        REQUIRE(page, "failed to get phys page for anon pool alloc bitmap");

        const auto virtAddr = allocBase + (i * pageSz);
        err = map->add(page, pageSz, virtAddr, vm::MapMode::kKernelRW | vm::MapMode::GLOBAL);
        REQUIRE(!err, "failed to map anon pool bitmap: %d", err);
    }

    this->freeMap = reinterpret_cast<uint32_t *>(allocBase);
    memset(this->freeMap, 0xFF, bitmapBytes);

    // mark the pages used for the bitmap as used
    for(size_t i = 0; i < bitmapPages; i++) {
        this->freeMap[i / 32] &= ~(1 << (i % 32));
    }
}

/**
 * Releases ALL physical memory pages in the alloc region of the pool.
 */
AnonPool::~AnonPool() {
    int err;
    uint64_t phys;

    const auto pageSz = arch_page_size();
    auto map = vm::Map::current();

    for(size_t i = 0; i < this->totalPages; i++) {
        err = map->get(this->virtBase + (pageSz * i), phys);
        if(!err) {
            mem::PhysicalAllocator::free(phys);
        }
    }
}


/**
 * Single page allocation helper that outputs the physical address of the page.
 */
void *AnonPool::allocPage(uint64_t &phys) {
    auto page = gShared->alloc(1);

    if(page) {
        auto map = vm::Map::current();

        int err = map->get((uintptr_t) page, phys);
        REQUIRE(!err, "failed to resolve phys addr: %d", err);
    }

    return page;
}

/**
 * Allocates a new range of pages from the pool.
 */
void *AnonPool::alloc(const size_t numPages) {
    int err;
    const auto pageSz = arch_page_size();
    auto map = vm::Map::current();

    size_t allocStart = 0;
    size_t allocPages = 0;

    // grab the free map lock
    SPIN_LOCK_GUARD(this->freeMapLck);

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

        // TODO: this might break across bounds of 32 pages
        for(size_t j = allocBit; j < 32; j++) {
            // if bit is set, add it to the set of available VM pages
            if(this->freeMap[i] & (1 << j)) {
                // if no pages have been allocated yet, signify this as the start
                if(!allocPages) {
                    allocStart = ((i * 32) + j);
                }

                // if we've allocated the desired number of pages, allocate for realsies
                if(++allocPages == numPages) {
                    goto beach;
                }
            } else {
                allocPages = 0;
                allocStart = 0;
            }
        }
    }

    // repeat search at the start of the region if nothing found
    if(first) {
        start = 0;
        first = false;
        goto search;
    }

    // if we get here, we failed to allocate a page
    return nullptr;

    // we've found a region to allocate starting at `allocStart`.
beach:;
    // allocate the physical memory pages
    uint64_t physPages[numPages];
    memset(&physPages, 0, sizeof(uint64_t) * numPages);

    for(size_t i = 0; i < numPages; i++) {
        physPages[i] = mem::PhysicalAllocator::alloc();
        if(!physPages[i]) goto fail;
    }

    // map each virtual page
    for(size_t i = 0; i < numPages; i++) {
        const auto idx = allocStart + i;

        // mark VM space as allocated
        this->freeMap[idx / 32] &= ~(1 << (idx % 32));

        // get virtual address and map it
        const auto virtAddr = this->virtBase + (idx * pageSz);

        err = map->add(physPages[i], pageSz, virtAddr, vm::MapMode::kKernelRW | vm::MapMode::GLOBAL);
        if(err) {
            // TODO: undo any previous virtual mappings!
            goto fail;
        }
    }

    // return vm addr of first page. also output phys addr of first page
    return (void *) (this->virtBase + (allocStart * pageSz));

fail:;
    // something went wrong. deallocate physical memory
     for(size_t i = 0; i < numPages; i++) {
         if(physPages[i]) {
             mem::PhysicalAllocator::free(physPages[i]);
         }
     }

     return nullptr;
}

/**
 * Releases a contiguous virtual mapping and the physical memory that backs it.
 */
void AnonPool::free(void *base, const size_t numPages) {
    uint64_t phys;
    int err;

    const auto pageSz = arch_page_size();
    auto map = vm::Map::current();

    // lock the free map and deallocate each page
    SPIN_LOCK_GUARD(this->freeMapLck);

    for(size_t i = 0; i < numPages; i++) {
        const uintptr_t virtAddr = ((uintptr_t) base) + (i * pageSz);
        const auto pageNo = (virtAddr - this->virtBase) / pageSz;

        // get the physical page that backs it
        err = map->get((uintptr_t) virtAddr, phys);
        REQUIRE(!err, "failed to resolve phys addr: %d", err);

        // unmap and mark this VM addr as available again
        err = map->remove((uintptr_t) virtAddr, arch_page_size());
        REQUIRE(!err, "failed to unmap anon page: %d", err);

        this->freeMap[pageNo / 32] |= (1 << (pageNo % 32));

        // free physical page
        mem::PhysicalAllocator::free(phys);
    }
}
