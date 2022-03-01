#ifndef KERNEL_MEMORY_PHYSICALALLOCATOR_H
#define KERNEL_MEMORY_PHYSICALALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

namespace Kernel {
namespace Memory {
class Pool;
}
namespace Vm {
class Map;
}

/**
 * @brief Dispenses physical memory with page granularity
 *
 * The physical allocator keeps track of all memory in the system, spread across one or more pools.
 * Inside each pool can be one or more regions, which are contiguous physical memory sections from
 * which physical pages are allocated.
 *
 * @remark Currently, all kernel requests are satisfied from the default pool. Any additional pools
 *         initialized by the platform code are not used.
 *
 * @remark All initialization must take place before any additional processors are started. That is
 *         to say, it is not threadsafe.
 */
class PhysicalAllocator {
    public:
        /// Maximum extra page sizes supported
        constexpr const static size_t kMaxExtraSizes{4};
        /// Maximum number of memory pools, including the default pool, to support
        constexpr const static size_t kMaxPools{4};

    public:
        static void Init(const size_t pageSz, const size_t extraSizes[],
                const size_t numExtraSizes, const size_t numBonusPools = 0);
        static void AddRegion(const uintptr_t base, const size_t length, const size_t pool = 0);

        static int AllocatePages(const size_t numPages, uintptr_t *outPageAddrs,
                const size_t pool = 0);
        static int FreePages(const size_t numPages, const uintptr_t *inPageAddrs,
                const size_t pool = 0);

        static size_t GetTotalPages(const size_t pool = 0);
        static size_t GetAllocPages(const size_t pool = 0);

        static void RemapTo(Kernel::Vm::Map *map);

        /// Get the primary page size of the physical allocator
        constexpr inline auto getPageSize() const {
            return this->pageSz;
        }

    private:
        PhysicalAllocator(const size_t pageSz, const size_t extraSizes[],
                const size_t numExtraSizes, const size_t numBonusPools);

    private:
        /// Globally shared instance of the physical allocator
        static PhysicalAllocator *gShared;

        /**
         * Size of a single page, in bytes
         *
         * @remark This must be a power of two.
         */
        size_t pageSz{0};

        /**
         * Sizes of additional "large" pages that can be allocated
         *
         * Each of these entries is a log2 value to apply against the existing page size. So, if a
         * system supports 4K pages, as well as 64K and 1M pages, this array would contain the
         * values `4, 8`.
         *
         * @remark Contents of this array should be sorted in ascending order.
         */
        uint8_t extraPageSizes[kMaxExtraSizes];

        /**
         * Memory pools to allocate from
         *
         * The first entry (primary pool) is guaranteed to always exist. It is the pool from which
         * most kernel allocations are satisfied, unless otherwise requested.
         */
        Memory::Pool *pools[kMaxPools];

        static_assert(kMaxExtraSizes >= 1, "invalid max extra page sizes");
        static_assert(kMaxPools >= 1, "invalid max pools size");
};
}

#endif
