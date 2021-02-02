#ifndef KERNEL_MEM_ANONPOOL_H
#define KERNEL_MEM_ANONPOOL_H

#include <stddef.h>
#include <stdint.h>

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
            uint64_t temp;
            return allocPage(temp);
        }
        /// Allocates a new page from the anon pool, getting its physical address
        static void *allocPage(uint64_t &phys) {
            return gShared->alloc(phys);
        }
        /// Releases a previously allocated page, given its virtual address
        static void freePage(void *virtAddr) {
            gShared->free(virtAddr);
        }

    private:
        AnonPool(const uintptr_t allocBase);

        static void init();

        void *alloc(uint64_t &physAddr);
        void free(void *virtAddr);

    private:
        /// base address of the anon pool region
        static const uintptr_t kBaseAddr = 0xC3000000;
        /// size of the anon pool region
        static const uintptr_t kRegionLength = (0xC8000000 - kBaseAddr);

        /// shared anon pool instance
        static AnonPool *gShared;

    private:
        /// TODO: better than just a sliding pointer lol
        uintptr_t nextAddr = kBaseAddr;
};
}

#endif
