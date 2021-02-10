#ifndef MEM_PHYSICALALLOCATOR_H
#define MEM_PHYSICALALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include <log.h>
#include <arch/spinlock.h>

namespace mem {

class PhysicalAllocator {
    public:
        static void init();
        static void vmAvailable() {
            gShared->mapRegionUsageBitmaps();
        }

        /// Returns the physical address of a newly allocated page, or 0 if no memory available
        static uint64_t alloc() {
            REQUIRE(gShared, "invalid allocator");
            return gShared->allocPage();
        }
        /// Frees a previously allocated physical page
        static void free(const uint64_t physicalAddr) {
            REQUIRE(gShared, "invalid allocator");
            gShared->freePage(physicalAddr);
        }

        /// Reserves the page at the given physical address.
        static void reserve(const uint64_t physicalAddr) {
            REQUIRE(gShared, "invalid allocator");
            gShared->reservePage(physicalAddr);
        }

    private:
        PhysicalAllocator();

        void mapRegionUsageBitmaps();

        uint64_t allocPage();
        void freePage(const uint64_t physicalAddr);
        void reservePage(const uint64_t physicalAddr);

    private:
        struct Region {
            /// base address (physical) of this memory allocation region
            uint64_t base = 0;
            /// size (in bytes) of this memory region
            uint64_t length = 0;

            /// number of allocatable pages (e.g. not used for accounting/metadata)
            size_t totalPages = 0;

            /// size of the free map, in pages (also: offset into region to allocation region)
            size_t freeMapPages = 0;
            /// pointer to the virtual address of the free map
            uint32_t *freeMap = nullptr;
            /// index of the next free page (used to optimize allocation)
            size_t nextFree = 0;

            Region() = default;
            Region(const uint64_t _base, const uint64_t _length) : base(_base), length(_length) {}

            /// Whether this region contains the given address
            bool contains(uint64_t address) const {
                return (address >= this->base) && (address <= (this->base + this->length));
            }

            /// returns the index of the next available page, and marks it allocated
            int alloc(uintptr_t *freeIdx);
            /// frees a page with the given index
            void free(const uintptr_t index);

            /// reserves the page at the given index
            void reserve(const uintptr_t index) {
                this->freeMap[index / 32] &= ~(1 << (index % 32));
            }
        };

    private:
        /// maximum number of physical regions to store info for
        constexpr static const size_t kMaxRegions = 5;

    private:
        static PhysicalAllocator *gShared;

    private:
        size_t numRegions = 0;
        Region regions[kMaxRegions];

        // TODO: this should be retrieved from the arch/platform code
        /// virtual address to map the next physical region allocation bitmap in
        uint32_t nextBitmapVmAddr = 0xC0400000;

        /// lock to protect the alloc bitmaps
        DECLARE_SPINLOCK(bitmapLock);
};

};

#endif
