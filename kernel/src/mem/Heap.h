#ifndef KERNEL_MEM_HEAP_H
#define KERNEL_MEM_HEAP_H

#include <stddef.h>
#include <stdint.h>

extern "C" void kernel_init();

namespace mem {
class Heap {
    friend void ::kernel_init();

    private:
        // define the extents of the heap region
#if defined(__amd64__)
        constexpr static const uintptr_t kHeapStart = 0xffff830000000000;
        constexpr static const uintptr_t   kHeapEnd = 0xffff83ffffffffff;
#else
#error Define heap address ranges
#endif

    public:
        static void *alloc(const size_t bytes);
        static void *allocAligned(const size_t bytes, const size_t align);
        static void *valloc(const size_t bytes);
        static void *pvalloc(const size_t bytes);

        static void *calloc(const size_t nItems, const size_t nBytes);
        static void *realloc(void *ptr, const size_t bytes);
        static void free(void *ptr);
        static void freeBulk(void **ptr, const size_t numPtr);

        // mmap() emulation for dlmalloc
        static void *fakeMmap(const size_t len);
        // munmap() emulation for dlmalloc
        static int fakeMunmap(const void *addr, const size_t len);

    private:
        static void init();

    private:
        /// whether heap allocations/deallocations are logged
        static bool gLogAlloc;
        /// whether heap mmap/munmap operations are logged
        static bool gLogMmap;

        /// base address for the next heap growth
        static uintptr_t gVmBase;
};
}

#endif
