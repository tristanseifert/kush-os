#include "Memory/Region.h"
#include "Memory/PhysicalAllocator.h"
#include "Memory/Pool.h"

#include "Logging/Console.h"

using namespace Kernel::Memory;

/**
 * Initialize a new region of physical memory.
 *
 * This sets up the associated bitmap: all pages are initially marked as available, except for
 * those used to actually store the bitmap itself.
 *
 * @param pool The allocation pool this region belongs to
 */
Region::Region(Pool *pool, const uintptr_t base, const size_t length) : physBase(base) {
    // convert length into pages
    this->numPages = length / pool->allocator->getPageSize();

    // TODO: allocate bitmap, etc.
    Console::Notice("Region: base %016llx, %zu pages", base, this->numPages);
}

/**
 * Attempt to allocate pages from this region.
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
    // TODO: implement
    return -1;
}

/**
 * Free the given pages.
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

    for(size_t i = 0; i < numPages; i++) {
        const auto addr = *inAddrs++;
        if(this->contains(addr)) {
            // TODO: free page
            freed++;
        }
    }

    return freed;
}
