#ifndef KERNEL_MEMORY_POOL_H
#define KERNEL_MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>

namespace Kernel {
class PhysicalAllocator;
namespace Vm {
class Map;
}
}

/// Physical allocator internals
namespace Kernel::Memory {
class Region;

/**
 * @brief Collection of allocatable physical regions
 *
 * Pools satisfy allocations from one or more of its regions, which are just contiguous sections of
 * physical memory space in the system.
 */
class Pool {
    friend class Kernel::PhysicalAllocator;
    friend class Region;

    public:
        /**
         * Maximum regions per pool
         *
         * This constant defines the maximum number of regions that can be associated with a single
         * pool, and will be used to satisfy all allocations in the pool.
         */
        constexpr static const size_t kMaxRegions{16};

        /**
         * Maximum global regions
         *
         * Since the physical allocator needs to function before any other memory allocation is
         * available, all of its structures are preallocated, including the regions. This constant
         * defines the maximum number of regions that can be allocated globally.
         */
        constexpr static const size_t kMaxGlobalRegions{48};

    private:
        Pool(PhysicalAllocator *allocator) : allocator(allocator) {}

        void addRegion(const uintptr_t base, const size_t length);
        void applyVirtualMap(Vm::Map *map);

    public:
        int alloc(const size_t num, uintptr_t *outAddrs);
        int free(const size_t num, const uintptr_t *inAddrs);

        size_t getTotalPages() const;
        size_t getAllocatedPages() const;

    private:
        /// Physical allocator that owns this pool
        PhysicalAllocator *allocator;

        /**
         * Regions belonging to the pool
         *
         * When allocating or deallocating, we'll iterate through this array until we either
         * encounter a `nullptr` or reach `kMaxRegions`, whichever happens first.
         */
        Region *regions[kMaxRegions];
};
}

#endif
