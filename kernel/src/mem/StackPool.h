#ifndef KERNEL_MEM_STACKPOOL_H
#define KERNEL_MEM_STACKPOOL_H

#include <stdint.h>

extern "C" void kernel_init();

namespace mem {
/**
 * Allocator for kernel stacks.
 *
 * This is similar to the anonymous allocator, except that we track regions by their bottom address
 * (since they're stacks, after all) and leave a 4K page at the top of the 32K region for each
 * stack unmapped as a guard page.
 */
class StackPool { 
    friend void ::kernel_init();

    public:
        /// Allocates a new kernel stack, returning its bottom (base) address
        static void *get() {
            return gShared->alloc();
        }
        /// Releases a previously allocated stack, given its base (bottom) address
        static void release(void *base) {
            gShared->free(base);
        }

    private:
        StackPool();

        static void init();

        /// returns an available stack
        void *alloc();
        /// releases the given stack
        void free(void *base);

    private:
        /// base address of the stack pool region
        static const uintptr_t kBaseAddr = 0xC1000000;
        /// size of the stack pool region
        static const uintptr_t kRegionLength = (0xC3000000 - kBaseAddr);

        /// size of a single stack region (in bytes)
        static const uintptr_t kStackSize = 0x8000;
        /// total number of stacks
        static const uintptr_t kNumStacks = kRegionLength / kStackSize;
        static_assert(!(kNumStacks & 0x1F), "Number of stacks must be a multiple of 32");

        /// shared anon pool instance
        static StackPool *gShared;

    private:
        /// bitmap for the stacks availability
        uint32_t freeMap[kNumStacks / 32];
};
}

#endif
