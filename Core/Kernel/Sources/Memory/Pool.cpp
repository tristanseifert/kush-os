#include "Memory/Pool.h"
#include "Memory/Region.h"

#include "Logging/Console.h"

#include <Platform.h>
#include <Intrinsics.h>
#include <new>

using namespace Kernel::Memory;

// space in .bss segment for regions
static KUSH_ALIGNED(64) uint8_t gRegionAllocBuf[Pool::kMaxGlobalRegions][sizeof(Region)];
// next free index in the allocation buffer
static size_t gRegionAllocBufNextFree{0};

/**
 * @brief Adds a region of physical memory to the pool.
 *
 * @param base Base address of the region
 * @param length Length of the region, in bytes
 *
 * @remark Base and length should be page aligned.
 */
void Pool::addRegion(const uintptr_t base, const size_t length) {
    REQUIRE(gRegionAllocBufNextFree < kMaxGlobalRegions, "region buffer exhausted");

    // find a free index
    size_t freeIndex{kMaxRegions};
    for(freeIndex = 0; freeIndex < kMaxRegions; freeIndex++) {
        if(!this->regions[freeIndex]) goto beach;
    }

beach:;
    REQUIRE(freeIndex < kMaxRegions, "pool cannot accept any more regions");

    // allocate and store it
    const auto idx = gRegionAllocBufNextFree++;
    auto ptr = reinterpret_cast<Region *>(gRegionAllocBuf[idx]);
    new (ptr) Region(this, base, length);

    // TODO: apply virtual memory mapping immediately if VM is already activated

    this->regions[freeIndex] = ptr;
}

/**
 * @brief Maps the VM objects of all regions' bitmaps/metadata into the specified map.
 *
 * The base addresses of each mapping are statically decided here, based on the base address
 * provided by the platform code. Bitmaps are placed sequentially, one after another, with a single
 * guard page between each allocation to catch out of bounds accesses.
 *
 * @param map Kernel memory map to receive the bitmaps.
 */
void Pool::applyVirtualMap(Vm::Map *map) {
    // start address of the bitmaps
    static uintptr_t gBitmapStart{Platform::KernelAddressLayout::PhysAllocatorMetadataStart};

    // map each region
    for(size_t i = 0; i < kMaxRegions; i++) {
        auto region = this->regions[i];
        if(!region) break;

        const auto used = region->applyVirtualMap(gBitmapStart, map);
        REQUIRE(used, "failed to map region %p", region);

        gBitmapStart += used + Platform::PageTable::PageSize();
        REQUIRE(gBitmapStart < Platform::KernelAddressLayout::PhysAllocatorMetadataEnd,
                "physical allocator metadata overflow (%016zx)", gBitmapStart);
    }
}

/**
 * @brief Attempt to allocate physical memory from this pool's regions.
 *
 * This consults each of the regions in the pool sequentially to satisfy however many pages are
 * remaining to allocate. So it's possible the allocations are satisfied from different regions of
 * physical memory.
 *
 * @param num Number of pages to allocate
 * @param outAddrs Buffer to receive the physical addresses of pages; it must be large enough to
 *        hold `num` addresses.
 *
 * @return Total number of pages allocated (which may be less than requested) or a negative error
 *         code
 */
int Pool::alloc(const size_t num, uintptr_t *outAddrs) {
    int allocated{0}, err;
    auto outPtr = outAddrs;

    for(size_t i = 0; i < kMaxRegions; i++) {
        auto region = this->regions[i];
        if(!region) break;

        // request allocation
        err = region->alloc(this, (num - allocated), outPtr);
        if(err < 0) goto error;

        allocated += err;

        // were all pages allocated?
        if(allocated == num) break;
        // otherwise, check the next entry into the region list
        region++;
    }

    return allocated;

error:;
    // an allocation failed; we'll have to free any existing pages
    if(allocated) {
        this->free(allocated, outAddrs);
    }

    return err;
}

/**
 * @brief Free the provided physical pages.
 *
 * @param num Number of pages to free
 * @param inAddrs Buffer containing physical addresses of pages to deallocate
 *
 * @return Total number of deallocated pages
 */
int Pool::free(const size_t num, const uintptr_t *inAddrs) {
    int freed{0};

    for(size_t i = 0; i < kMaxRegions; i++) {
        auto region = this->regions[i];
        if(!region) break;

        freed += region->free(this, num, inAddrs);
        if(freed == num) break;
    }

    return freed;
}

/**
 * @brief Get the total number of physical pages available across all regions.
 *
 * @return Total physical pages available to all regions in this pool
 */
size_t Pool::getTotalPages() const {
    size_t sum{0};
    for(size_t i = 0; i < kMaxRegions; i++) {
        auto region = this->regions[i];
        if(!region) break;

        sum += region->bitmapSize;
    }
    return sum;
}

/**
 * @brief Get the total number of allocated physical pages.
 *
 * @return Number of physical pages currently allocated
 */
size_t Pool::getAllocatedPages() const {
    size_t sum{0};
    for(size_t i = 0; i < kMaxRegions; i++) {
        auto region = this->regions[i];
        if(!region) break;

        sum += region->numAllocated;
    }
    return sum;
}
