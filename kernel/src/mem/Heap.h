#ifndef KERNEL_MEM_HEAP_H
#define KERNEL_MEM_HEAP_H

#include <stddef.h>

extern "C" void kernel_init();

namespace mem {
class Heap {
    friend void ::kernel_init();

    public:
        static void *alloc(const size_t bytes);
        static void *calloc(const size_t nItems, const size_t nBytes);
        static void *realloc(void *ptr, const size_t bytes);
        static void free(void *ptr);

    private:
        static void init();
};
}

#endif
