#include <sys/syscalls.h>
#include <malloc.h>
#include <errno.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static uintptr_t gHeapHandle = 0;
static uintptr_t gHeapStart = 0;
static size_t gHeapSize = 0;

static void *gSbrkBase = NULL;

/// VM base of the heap
#if defined(__i386__)
LIBC_INTERNAL static uintptr_t gHeapInitialAddr = 0x30000000;
static const size_t kHeapMaxSize =  0x10000000; // 1 gig
#elif defined(__amd64__)
LIBC_INTERNAL static uintptr_t gHeapInitialAddr = 0x1000000000;
static const size_t kHeapMaxSize =  0x1000000000; // 64 gigs
#endif

/// Maximum heap size: 0 indicates unlimited (we'll grow til we hit something else)
LIBC_INTERNAL static size_t gHeapMaxSize = kHeapMaxSize;

/**
 * Adjusts the initial address of the heap.
 *
 * This is a private interface for use by the dynamic linker, so that it can use the regular static
 * version of the C library; it simply calls this with a starting address high in memory that will
 * not conflict with the heap in an executable.
 *
 * This must be called before any other libc function, INCLUDING __libc_init, is called.
 */
LIBC_EXPORT void __libc_set_heap_start(const uintptr_t start, const size_t maxSize) {
    gHeapInitialAddr = start;
    gHeapMaxSize = (maxSize > kHeapMaxSize) ? kHeapMaxSize : maxSize;
}

/**
 * Initializes the sbrk emulation.
 */
LIBC_INTERNAL void __fake_sbrk_init(const size_t initialSize) {
    int err;

    // allocate a region for the heap
    err = AllocVirtualAnonRegion(initialSize, VM_REGION_RW, &gHeapHandle);
    if(err) {
        abort();
    }

    // create a view into that region with the maximum heap size
    err = MapVirtualRegion(gHeapHandle, gHeapInitialAddr, kHeapMaxSize, 0);
    if(err) {
        abort();
    }

    gHeapStart = gHeapInitialAddr;
    gSbrkBase = (void *) gHeapInitialAddr;
    gHeapSize = initialSize;
}

/**
 * Fake implementation of the sbrk system call for use with our internal memory allocator and
 * friends.
 */
LIBC_INTERNAL void *__fake_sbrk(const intptr_t inc) {
    int err;

    // fail if we're at or above the limit
    if(gHeapMaxSize && gHeapSize >= gHeapMaxSize) {
        errno = ENOMEM;
        return (void *) -1;
    }

    // if increment is 0, return current sbrk base
    if(!inc) {
        return gSbrkBase;
    }
    // allocate the region if needed
    if(!gHeapHandle) {
        // use the increment as its initial size
        if(inc > 0) {
            __fake_sbrk_init(inc);
            return gSbrkBase;
        }
        // we should never get here!
        else {
            return (void *) -1;
        }
    }

    // if negative, attempt to shrink the heap region
    if(inc < 0) {
        fprintf(stderr, "shrinkn't\n");
        // XXX: no idea if this is right
        abort();
        // get the current pointer to the end of the region
        void *endPtr = ((void *) (gHeapStart + gHeapSize));

        // resize it
        const size_t newSize = gHeapSize - labs(inc);
        err = ResizeVirtualRegion(gHeapHandle, newSize);
        if(err) {
            abort();
        }
        gHeapSize = newSize;

        gHeapSize = newSize;

        // return the pointer to the end of the region, i.e. where this new memory begins
        gSbrkBase = (void *) endPtr;
        return endPtr;
    }
    // otherwise, grow it
    else {
        // get the current pointer to the end of the region
        void *endPtr = ((void *) (gHeapStart + gHeapSize));

        // resize it
        const size_t newSize = gHeapSize + inc;
        err = ResizeVirtualRegion(gHeapHandle, newSize);
        if(err) {
            abort();
        }
        gHeapSize = newSize;

        // return the pointer to the end of the region, i.e. where this new memory begins
        gSbrkBase = (void *) endPtr;
        return endPtr;
    }

    // failed to update allocation if we get here
    return (void *) -1;
}
