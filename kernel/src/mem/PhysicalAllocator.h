#ifndef MEM_PHYSICALALLOCATOR_H
#define MEM_PHYSICALALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <bitflags.h>

#include <log.h>
#include <arch/spinlock.h>

#include "PhysRegion.h"

namespace mem {
/**
 * Flags for allocations
 */
ENUM_FLAGS(PhysFlags)
enum class PhysFlags {
    None                                = 0,
};


/**
 * Provides an interface for allocating contiguous chunks of physical memory.
 */
class PhysicalAllocator {
    public:
        static void init();
        static void vmAvailable() {
            gShared->notifyRegionsVm();
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

        /// Returns the total number of pages
        static size_t getTotalPages();
        /// Returns the number of allocated pages
        static size_t getAllocPages() {
            size_t ret = 0;
            __atomic_load(&gShared->allocatedPages, &ret, __ATOMIC_RELAXED);
            return ret;
        }
        /// Returns the number of reserved pages
        static size_t getReservedPages() {
            size_t ret = 0;
            __atomic_load(&gShared->reservedPages, &ret, __ATOMIC_RELAXED);
            return ret;
        }

    private:
        PhysicalAllocator();

        void notifyRegionsVm();

        uint64_t allocPage(const PhysFlags flags = PhysFlags::None);
        void freePage(const uint64_t physicalAddr);

    public:
        /// maximum number of physical regions to store info for
        constexpr static const size_t kMaxRegions = 10;

        // TODO: this should be retrieved from the arch/platform code
        /// virtual address to map the next physical region allocation bitmap in
#if defined(__amd64__)
        constexpr static const uintptr_t kRegionInfoBase = 0xffff82ff00000000;
        constexpr static const size_t kRegionInfoEntryLength = 0x10000000; // up to 16x
#endif

    private:
        static PhysicalAllocator *gShared;

        /// do we log all physical regions that are skipped?
        static bool gLogSkipped;
        /// do we log all newly allocated regions?
        static bool gLogRegions;

    private:
        size_t numRegions{0};
        /// regions from which ~ memory ~ can be acquired
        PhysRegion *regions[kMaxRegions];

        /// number of allocated a pges
        size_t allocatedPages{0};
        /// number of reserved pages
        size_t reservedPages{0};
};

};

#endif
