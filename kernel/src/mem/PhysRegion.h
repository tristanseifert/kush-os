#ifndef KERNEL_MEM_PHYSREGION_H
#define KERNEL_MEM_PHYSREGION_H

#include <stddef.h>
#include <stdint.h>

#include <arch/spinlock.h>

namespace mem {
/**
 * Encapsulates a single contiguous physical region of memory, from which page granularity
 * allocations can be made.
 *
 * Each region can represent up to 96Mbytes of 4K pages. Each page is represented by a bit in a
 * bitmap. If a region contains more than these 24k pages, additional region structs can be
 * chained together.
 *
 * TODO: add locking and stuff to make this work on multiprocessor systems
 */
class PhysRegion {
    public:
        /// Can we create a region from the given range?
        static bool CanAllocate(const uint64_t base, const uint64_t len);

        /// Constructs a new, empty phys region
        PhysRegion() = default;
        /// Create a new physical region
        PhysRegion(const uint64_t base, const uint64_t len);

        /// allocate further chunks for additional memory beyond the first 24k pages
        void initNextIfNeeded();

        /// Allocates a single physical page.
        uint64_t alloc();
        /// Attempt to allocate the given number of physical pages.
        int alloc(const size_t numPages, uint64_t * _Nonnull outPages);

        /// Releases a single page.
        inline void free(const uint64_t address) {
            const uint64_t pages[1] = { address };
            this->free(pages, 1);
        }
        /// Releases the given pages (identified by their physical address)
        int free(const uint64_t * _Nonnull pages, const size_t numPages);

        /// Check if the given physical address was allocated from this region.
        inline bool checkAddress(const uintptr_t addr) const {
            return (addr >= this->base) && (addr < (this->base + this->allocatable));
        }

    private:
        /// creates a child physical region
        PhysRegion(PhysRegion * _Nonnull parent, const uint64_t offset);

        /// shared initialization of internal data structure
        void commonInit(const size_t numPages);
        /// fills a phys memory page with zeros
        void zero(const uint64_t physAddr);

    private:
#if defined(__amd64__)
        /// Base address of physical memory identity mapping zone during early boot
        constexpr static const uintptr_t kEarlyPhysIdentityMap = 0x0000000000000000;
        /// Base address of the physical memory identity mapping zone
        constexpr static const uintptr_t kPhysIdentityMap = 0xffff800000000000;
#endif

        /// minimum number of pages a region must contain
        constexpr static const size_t kMinPages = 4;
        /// maximum number of pages a region can hold
        constexpr static const size_t kMaxPages = 24576;

    private:
        /// whether initialization info is logged
        static bool gLogInit;
        /// When set, all page allocations are logged
        static bool gLogAlloc;
        /// whether page frees are logged
        static bool gLogFree;

    private:
        DECLARE_SPINLOCK(lock);

        /// whether virtual memory is available
        bool vmAvailable = false;

        /// base address (physical) of this memory allocation region
        uint64_t base = 0;
        /// size (in bytes) of this memory region
        uint64_t length = 0;

        /// usable, allocatable storage (in bytes) inside this region
        uint64_t allocatable = 0;

        /// highest valid bit index in bitmap
        size_t bitmapMax = 0;
        /// allocation bitmap: 1 is free, 0 is allocated
        uint64_t bitmap[kMaxPages / 8 / sizeof(uint64_t)] {0};

    public:
        /// If there is another physical region, pointer to it
        PhysRegion * _Nullable next = nullptr;
        /// Parent (head of list) region
        PhysRegion * _Nullable parent = nullptr;
};
}

#endif
