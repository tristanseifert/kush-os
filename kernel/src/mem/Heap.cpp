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
    return kmalloc(bytes);
}
/**
 * Frees a previous heap allocation.
 */
void Heap::free(void *ptr) {
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

