#ifndef KERNEL_MEMORY_REGION_H
#define KERNEL_MEMORY_REGION_H

#include <stddef.h>
#include <stdint.h>

namespace Kernel {
class PhysicalAllocator;
}

namespace Kernel::Memory {
class Pool;

/**
 * Contiguous segment of physical memory, from which pages may be allocated
 *
 * Internally, each region reserves some part of the physical pages it manages as a bitmap, used to
 * indicate which pages are allocated, and which are used.
 */
class Region {
    friend class Pool;

    private:
        Region(Pool *pool, const uintptr_t base, const size_t length);

        int alloc(Pool *pool, const size_t numPages, uintptr_t *outAddrs);
        int free(Pool *pool, const size_t numPages, const uintptr_t *inAddrs);

        /**
         * Test if the given physical page address is contained in this region.
         *
         * @param address Physical address to test
         *
         * @return Whether the address is inside this region.
         */
        constexpr inline bool contains(const uintptr_t address) {
            return (address >= this->physBase) && (address < (this->physBase + this->numPages));
        }

    private:
        /// Physical base address of the region
        uintptr_t physBase;

        /**
         * Total number of pages in this region
         *
         * @remark This does not equal the total number of allocatable pages; some of these pages
         *         will be reserved for metadata.
         */
        size_t numPages;
};
}

#endif
