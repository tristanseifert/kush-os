#include "PhysRegion.h"

#include <stdint.h>
#include <string.h>

#include <arch.h>
#include <log.h>

using namespace mem;

/**
 * Determines whether a given physical memory range is suitable for adding to the physical page
 * allocator.
 *
 * Currently, this just requires it's at least as large as one block in the maximum order, plus a
 * small fudge factor to allow for metadata to be placed. This means that blocks smaller than 4M
 * of physical memory will simply be ignored; hopefully, real systems have reasonably contiguous
 * memory maps.
 */
bool PhysRegion::CanAllocate(const uint64_t base, const uint64_t len) {
    const auto pageSz = arch_page_size();

    size_t bytesRequired = (pageSz * (1 << kMaxOrder));
    bytesRequired += (64 * 1024);
    return (len > bytesRequired);
}


/**
 * Initializes a new physical allocation region.
 *
 * We'll set up structures for free lists of each order (up to the maximum order, or the largest
 * order that will fit in the region entirely) and allocate their page bitmaps.
 *
 * XXX: The allocation logic probably will have weird edge cases for regions smaller than the max
 * order, or those that are right on the boundaries. It's best to probably just ignore such
 * region for now.
 */
PhysRegion::PhysRegion(const uint64_t _base, const uint64_t _length) : base(_base),
    length(_length) {
    // calculate total number of pages and max order
    const auto pageSz = arch_page_size();
    const auto pages = this->length / pageSz;

    size_t maxOrder = kMaxOrder;
    if(Order::size(maxOrder - 1) > _length) {

    }

    log("max order for region at $%p (len %u bytes/%u pages): %u", this->base, this->length, pages, maxOrder);

    // allocate each order and its bitmaps
    //  size_t bitmapBytes = 0;
    for(size_t i = 0; i < maxOrder; i++) {
        // 
    }
}

/**
 * Remaps structures into the appropriate virtual memory addresses.
 *
 * @param base Virtual address at which the next VM mapping for the physical region shall be made
 * @return Number of bytes of virtual address space used (must be page aligned)
 */
uintptr_t PhysRegion::vmAvailable(const uintptr_t base) {
    size_t used = 0;

    // apply fixups for any allocated orders
    for(size_t i = 0; i < kMaxOrder; i++) {
        auto &order = this->orders[i];

        // skip unallocated orders
        if(!order.numBlocks) {
            break;
        }

        // update allocations
        used += order.vmAvailable(base + used);
    }

    // done
    return used;
}

/**
 * Maps the block state bitmap into virtual memory. If there are any entries in the free list, we
 * will move these to virtual memory as well.
 *
 * @param base Virtual address at to base mappings at
 * @return Number of bytes of virtual address space used (must be page aligned)
 */
uintptr_t PhysRegion::Order::vmAvailable(const uintptr_t base) {
    // TODO: implement
    return 0;
}



/**
 * Returns the size, in bytes, of a single block in the given order.
 */
size_t PhysRegion::Order::size(const size_t order) {
    REQUIRE(order < kMaxOrder, "invalid order: %u", order);

    const auto pageSz = arch_page_size();
    return pageSz * (order * order);
}
