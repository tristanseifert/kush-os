#ifndef KERNEL_MEM_PHYSREGION_H
#define KERNEL_MEM_PHYSREGION_H

#include <stddef.h>
#include <stdint.h>

namespace mem {
/**
 * Encapsulates a single contiguous physical region of memory, from which page granularity
 * allocations can be made.
 *
 * This is implemented as a buddy system, with a configurable maximum order.
 */
class PhysRegion {
    public:
        /**
         * Maximum page order to use in the allocator
         *
         * The order is defined as a power-of-two multiplier on the page size: so, an allocation of
         * order 0 is one page, one of order 2 is 4 pages, and so forth (2^n * pageSize)
         *
         * This is, in other words, the size of the largest allocation we can service.
         */
        constexpr static const size_t kMaxOrder = 10;

    public:
        /// Can we create a region from the given range?
        static bool CanAllocate(const uint64_t base, const uint64_t len);

        /// Create a new physical region
        PhysRegion(const uint64_t base, const uint64_t len);

        /// VM system is available, map all structures.
        uintptr_t vmAvailable(uintptr_t base);

    private:
        /**
         * Information structure for a single order of pages. This is a power-of-two sized region
         * that maintains both a linked list of free pages, and a bitmap of the state of each of
         * the buddies of a block.
         */
        struct Order {
            /// head of free list

            /**
             * Block status bitmap
             *
             * In this map, whenever a buddy is allocated or freed, its bit is toggled. So, a bit
             * that is set to 0 indicates both sets of pages in the buddies are free or in use,
             * while a 1 bit indicates only one of the two buddies is allocated.
             *
             * Each pair of buddies maps to a single bit.
             */
            uint8_t *bitmap = nullptr;

            /// number of allocatable blocks
            size_t numBlocks = 0;

            /// Gets the size, in bytes, of the given order.
            static size_t size(const size_t order);
            /// Applies fixups to virtual addresses, if needed; return number of bytes of VM used
            uintptr_t vmAvailable(const uintptr_t vmBase);
        };

    private:
        /// base address (physical) of this memory allocation region
        uint64_t base;
        /// size (in bytes) of this memory region
        uint64_t length;
        /// number of allocatable pages (e.g. not used for accounting/metadata)
        size_t totalPages = 0;

        /// Holds data for each order of pages we can allocate.
        Order orders[kMaxOrder];
};
}

#endif
