#ifndef KERNEL_MEMORY_REGION_H
#define KERNEL_MEMORY_REGION_H

#include <stddef.h>
#include <stdint.h>

namespace Kernel {
class PhysicalAllocator;

namespace Vm {
class Map;
class MapEntry;
}
}

namespace Kernel::Memory {
class Pool;

/**
 * @brief Contiguous segment of physical memory from which pages may be allocated
 *
 * Internally, each region reserves some part of the physical pages it manages as a bitmap, used to
 * indicate which pages are allocated, and which are unused. The bitmap is represented such that
 * all pages that are free are set (1) and allocated pages are cleared (0)
 *
 * @TODO Add locking
 * @TODO Per CPU caches
 */
class Region {
    friend class Pool;

    protected:
        Region(Pool *pool, const uintptr_t base, const size_t length);
        ~Region();

        int alloc(Pool *pool, const size_t numPages, uintptr_t *outAddrs);
        int free(Pool *pool, const size_t numPages, const uintptr_t *inAddrs);

        size_t applyVirtualMap(uintptr_t base, Vm::Map *map);

        /**
         * @brief Test if the given physical page address is contained in this region.
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

        /**
         * Physical address of the bitmap
         *
         * The physical address should be page aligned, and refers to the first byte in the bitmap
         * of allocated pages.
         */
        uintptr_t bitmapPhys;

        /**
         * Number of entries (bits) in the bitmap
         *
         * Also known as the total number of allocatable pages in the region
         */
        size_t bitmapSize;

        /**
         * Amount of bytes reserved for bitmap
         *
         * Bitmap is allocated in increments of whole pages, so the amount reserved for it will
         * often be greater than the actual space required.
         */
        size_t bitmapReserved;

        /**
         * Virtual address of the bitmap
         *
         * The bitmap should be mapped somewhere in virtual address space where the kernel can
         * always access it. This points to the first byte of the bitmap. The bitmap is accessed in
         * machine word sized chunks for optimum performance.
         */
        uint64_t *bitmap{nullptr};

        /**
         * Virtual memory object (in ther kernel map) for the bitmap
         *
         * This object is either provided as part of the constructor of the region, or allocated by
         * the constructor; and is mapped into the kernel virtual memory map.
         */
        Vm::MapEntry *bitmapVm{nullptr};

        /**
         * Physical address of the first allocatable page
         *
         * The first bit in the bitmap corresponds to the memory page at this physical address. It
         * takes into account the space reserved at the start of the region for metadata (such as
         * the bitmap.)
         */
        uintptr_t allocBasePhys;

        /// Number of allocated pages
        size_t numAllocated{0};
};
}

#endif
