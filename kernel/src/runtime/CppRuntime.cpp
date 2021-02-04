#include <stddef.h>
#include <log.h>

#include "mem/Heap.h"

extern "C" void __cxa_pure_virtual();

/**
 * Virtual function stub
 */
extern "C" void __cxa_pure_virtual() {
    panic("pure virtual function called");
}

/// Allocates memory
void *operator new(size_t size) {
    return mem::Heap::alloc(size);
}
/// Allocates an array
void *operator new[](size_t size) {
    return mem::Heap::alloc(size);
}
/// release previously allocated memory
void operator delete(void *p) {
    mem::Heap::free(p);
}
/// release previously allocated array memory
void operator delete[](void *p) {
    mem::Heap::free(p);
}
