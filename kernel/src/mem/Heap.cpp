#include "Heap.h"
#include "AnonPool.h"

#include <arch.h>
#include <arch/rwlock.h>
#include <arch/spinlock.h>
#include <log.h>

#include "liballoc/liballoc.h"
#include "liballoc/internal.h"

using namespace mem;

DECLARE_SPINLOCK_S(liballoc);

/// Whether allocations are logged
static bool gLogAlloc = false;

/**
 * Initializes the heap.
 */
void Heap::init() {
    liballoc_set_pagesize(arch_page_size());

    // test allocation (to force heap init)
    void *test = kmalloc(64);
    REQUIRE(test, "kmalloc test failed");

    kfree(test);
}

/**
 * Performs an allocation from the heap.
 */
void *Heap::alloc(const size_t bytes) {
    auto ptr = kmalloc(bytes);
    if(gLogAlloc) log("kmalloc(%u) = %p", bytes, ptr);
    return ptr;
}
/**
 * Allocates some items and ensures the memory is zeroed.
 */
void *Heap::calloc(const size_t nItems, const size_t nBytes) {
    auto ptr = kcalloc(nItems, nBytes);
    if(gLogAlloc) log("kcalloc(%u, %u) = %p", nItems, nBytes, ptr);
    return ptr;
}
/**
 * Resizes an existing allocation.
 */
void *Heap::realloc(void *ptr, const size_t bytes) {
    auto outPtr = krealloc(ptr, bytes);
    if(gLogAlloc) log("krealloc(%p, %u) = %p", ptr, bytes, outPtr);
    return outPtr;
}
/**
 * Frees a previous heap allocation.
 */
void Heap::free(void *ptr) {
    if(gLogAlloc) log("kfree(%p)", ptr);
    kfree(ptr);
}


/**
 * Locks the liballoc data structures.
 */
int liballoc_lock() {
    SPIN_LOCK(liballoc);
    return 0;
}

/**
 * Unlocks the liballoc data structures.
 */
int liballoc_unlock() {
    SPIN_UNLOCK(liballoc);
    return 0;
}

/**
 * Allocates the given number of pages for the heap from the anon pool.
 */
void *liballoc_alloc(const size_t numPages) {
    return AnonPool::allocPages(numPages);
}

/**
 * Frees a previously allocated region of heap memory. We just return the memory back to the anon
 * pool.
 */
int liballoc_free(void *base, const size_t numPages) {
    AnonPool::freePages(base, numPages);
    return 0;
}

