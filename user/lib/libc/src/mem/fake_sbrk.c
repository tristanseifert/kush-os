#include <sys/syscalls.h>
#include <malloc.h>
#include <errno.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

static uintptr_t gHeapHandle = 0;
static uintptr_t gHeapStart = 0;
static size_t gHeapSize = 0;

static void *gSbrkBase = NULL;

/// VM base of the heap
static const uintptr_t kHeapInitialAddr = 0x30000000;

/**
 * Initializes the sbrk emulation.
 */
void __fake_sbrk_init(const size_t initialSize) {
    int err;

    // allocate a region for the heap
    err = AllocVirtualAnonRegion(kHeapInitialAddr, initialSize, VM_REGION_RW, &gHeapHandle);
    if(err) {
        abort();
    }

    gHeapStart = kHeapInitialAddr;
    gSbrkBase = (void *) kHeapInitialAddr;
    gHeapSize = initialSize;
}

/**
 * Fake implementation of the sbrk system call for use with our internal memory allocator and
 * friends.
 */
void *__fake_sbrk(const intptr_t inc) {
    int err;

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
        // TODO: implement
        abort();
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
