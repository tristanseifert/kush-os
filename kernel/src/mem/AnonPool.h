#ifndef KERNEL_MEM_ANONPOOL_H
#define KERNEL_MEM_ANONPOOL_H

#include <stddef.h>
#include <stdint.h>

#include <arch/spinlock.h>

extern "C" void kernel_init();

namespace mem {
/**
 * The anon pool is a region of kernel memory from which various subsystems can allocate whole
 * pages.
 */
class AnonPool {
    friend void ::kernel_init();

    public:
        /// Allocates a new page from the anon pool
        static void *allocPage() {
            return gShared->alloc(1);
        }
        /// Allocates a new page from the anon pool, getting its physical address
        static void *allocPage(uint64_t &phys);
        /// Releases a previously allocated page, given its virtual address
        static void freePage(void *virtAddr) {
            gShared->free(virtAddr, 1);
        }

        /// Allocates a range of pages. The base address of the allocated region is returned.
        static void *allocPages(const size_t numPages) {
            return gShared->alloc(numPages);
        }
        /**
         * Frees the given number of pages starting at the specified address. There is no checking
         * performed here!
         */
        static void freePages(void *base, const size_t numPages) {
            gShared->free(base, numPages);
        }

    private:
        AnonPool(const uintptr_t allocBase, const size_t numPages);
        ~AnonPool();

        static void init();

        void *alloc(const size_t numPages = 1);
        void free(void *virtAddr, const size_t numPages = 1);

    private:
#if defined(__i386__)
        /// base address of the anon pool region
        static const uintptr_t kBaseAddr = 0xC8000000;
        /// size of the anon pool region
        static const uintptr_t kRegionLength = (0xF0000000 - kBaseAddr);
#elif defined(__amd64__)
        /// base address of the anon pool region
        static const uintptr_t kBaseAddr = 0xFFFF821000000000;
        /// size of the anon pool region (2G for now)
        static const uintptr_t kRegionLength = 0x80000000;
#else
#error Unsupported architecture
#endif

        /// shared anon pool instance
        static AnonPool *gShared;

        /// Whether memory pages returned by the pool are mapped as global or not
        static bool gMapGlobal;

    private:
        /// spin lock for the entire free map
        DECLARE_SPINLOCK(freeMapLck);

        /// total number of allocatable virtual pages
        size_t totalPages;
        /// free bitmap
        uint32_t *freeMap = nullptr;
        /// index of the next free page
        size_t nextFree = 0; 

        /// base address of the virtual allocation region
        uintptr_t virtBase;
};
}

#endif
