#include <stddef.h>
#include <stdint.h>
#include <log.h>

#include <new>

#include "mem/Heap.h"

// kinda hacky lol
namespace std {
enum class align_val_t : size_t {};
}

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
/// Aligned allocation
void *operator new(size_t size, std::align_val_t al) {
    void *result = mem::Heap::alloc(size);

    //log("requesting aligned alloc for size %zu (align %zu) %p", size, (size_t) al, result);
    return result;
}
/// Allocates an array
void *operator new[](size_t size) {
    return mem::Heap::alloc(size);
}
/// release previously allocated memory
void operator delete(void *p) {
    mem::Heap::free(p);
}
/// release previously allocated memory
void operator delete(void *p, size_t length) {
    // TODO: can we pass the length in for sanity checking?
    mem::Heap::free(p);
}
/// release previously allocated array memory
void operator delete[](void *p) {
    mem::Heap::free(p);
}
/// release aligned memory
void operator delete(void *p, std::align_val_t al) {
    mem::Heap::free(p);
}
