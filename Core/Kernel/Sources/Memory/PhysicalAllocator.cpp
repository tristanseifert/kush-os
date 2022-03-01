#include "Memory/PhysicalAllocator.h"
#include "Memory/Pool.h"

#include "Logging/Console.h"
#include "Runtime/String.h"

#include <Intrinsics.h>
#include <new>

using namespace Kernel;

// space in .bss segment for the physical allocator
static KUSH_ALIGNED(64) uint8_t gPhysAllocBuf[sizeof(PhysicalAllocator)];
// space in .bss segment for pools
static KUSH_ALIGNED(64) uint8_t gPoolAllocBuf[PhysicalAllocator::kMaxPools][sizeof(Memory::Pool)];

PhysicalAllocator *PhysicalAllocator::gShared{nullptr};


/**
 * Initialize the global physical allocator.
 *
 * @remark Page sizes (both the base and extra sizes) should be powers of two.
 *
 * @param pageSz Standard page size, in bytes
 * @param extraSizes An array of additional page sizes, or `nullptr` if the platform does not
 *        support additional page sizes. Each entry should be in bytes.
 * @param numExtraSizes Number of extra page sizes specified
 * @param numBonusPools Number of additional pools (beyond the default) to allocate
 */
void PhysicalAllocator::Init(const size_t pageSz, const size_t extraSizes[],
        const size_t numExtraSizes, const size_t numBonusPools) {
    auto ptr = reinterpret_cast<PhysicalAllocator *>(&gPhysAllocBuf);
    new (ptr) PhysicalAllocator(pageSz, extraSizes, numExtraSizes, numBonusPools);

    REQUIRE(!gShared, "cannot re-initialize physical allocator");
    gShared = ptr;
}

/**
 * Initialize the physical allocator.
 */
PhysicalAllocator::PhysicalAllocator(const size_t _pageSz, const size_t extraSizes[],
        const size_t numExtraSizes, const size_t numBonusPools) : pageSz(_pageSz) {
    memset(this->pools, 0, sizeof(this->pools));

    // validate page sizes are a power of 2
    REQUIRE(__builtin_popcount(_pageSz) == 1, "non power of 2 page size: %llx", _pageSz);
    const size_t base = __builtin_ffsll(_pageSz) - 1;

    if(extraSizes && numExtraSizes) {
        REQUIRE(numExtraSizes <= kMaxExtraSizes, "too many extra page sizes (max %zu, got %zu)",
                kMaxExtraSizes, numExtraSizes);
        for(size_t i = 0; i < numExtraSizes; i++) {
            const auto size = extraSizes[i];
            REQUIRE(__builtin_popcount(size) == 1, "non power of 2 page size: %llx", size);

            // convert to power of two
            const auto power = (__builtin_ffsll(size) - 1) - base;
            this->extraPageSizes[i] = power;
        }
    }

    // initialize the primary pool
    auto first = reinterpret_cast<Memory::Pool *>(&gPoolAllocBuf[0]);
    new (first) Memory::Pool(this);

    this->pools[0] = first;

    // TODO: initialize secondary/bonus pools
}

/**
 * Initialize a new region.of physical memory and adds it to a pool.
 *
 * @param base Physical base address (must be page aligned)
 * @param length Length of the region, in bytes
 * @param pool Pool to add the region to
 */
void PhysicalAllocator::AddRegion(const uintptr_t base, const size_t length, const size_t pool) {
    REQUIRE(!(base & (gShared->pageSz - 1)), "invalid region %s: %016lx", "base", base);
    REQUIRE(length && !(length & (gShared->pageSz - 1)), "invalid region %s: %016lx", "length",
            length);
    REQUIRE(pool < kMaxPools, "invalid pool");

    gShared->pools[pool]->addRegion(base, length);
}

/**
 * Allocate some physical pages of the standard page size
 *
 * @param numPages Number of pages to allocate
 * @param outPageAddrs A buffer to hold the resulting physical addresses; it must have space for
 *        at least `numPages` entries.
 * @param pool Index of the pool to allocate from
 *
 * @return The number of actually allocated pages, or a negative error code.
 */
int PhysicalAllocator::AllocatePages(const size_t numPages, uintptr_t *outPageAddrs,
        const size_t pool) {
    REQUIRE(numPages && outPageAddrs, "invalid page address buffer");
    REQUIRE(pool < kMaxPools, "invalid pool");

    return gShared->pools[pool]->alloc(numPages, outPageAddrs);
}

/**
 * Releases all physical memory pages specified.
 *
 * @param numPages Number of pages to release
 * @param inPageAddrs Buffer containing `numPages` physical page addresses
 * @param pool Index of the pool from which all of these pages were allocated
 *
 * @return Number of pages actually freed
 *
 * @remark All page addresses specified must have been allocated from the same pool of physical
 *         memory.
 */
int PhysicalAllocator::FreePages(const size_t numPages, const uintptr_t *inPageAddrs,
        const size_t pool) {
    REQUIRE(numPages && inPageAddrs, "invalid page address buffer");
    REQUIRE(pool < kMaxPools, "invalid pool");

    return gShared->pools[pool]->free(numPages, inPageAddrs);
}
