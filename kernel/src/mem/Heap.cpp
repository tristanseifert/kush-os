#include "Heap.h"

#include <arch.h>
#include <arch/spinlock.h>

#include <log.h>

#include "mem/PhysicalAllocator.h"
#include "sched/Task.h"
#include "vm/Map.h"
#include "vm/MapEntry.h"

#include "dlmalloc/dlmalloc.h"

using namespace mem;

bool Heap::gLogAlloc = false;
bool Heap::gLogMmap = false;

/// base address at which heap allocations begin
uintptr_t Heap::gVmBase = Heap::kHeapStart;

/// Heap lock (to protect access to mem map)
DECLARE_SPINLOCK_S(gHeapLock);

/**
 * Initializes the heap.
 */
void Heap::init() {
    // test allocation (to force heap init)
    void *test = dlmalloc(64);
    REQUIRE(test, "dlmalloc test failed");

    dlfree(test);
}

/**
 * Performs an allocation from the heap.
 */
void *Heap::alloc(const size_t bytes) {
    auto ptr = dlmalloc(bytes);
    if(gLogAlloc) log("%s(%u) = %p", "dlmalloc", bytes, ptr);
    return ptr;
}
/**
 * Performs an aligned allocation from the heap.
 */
void *Heap::allocAligned(const size_t bytes, const size_t alignment) {
    auto ptr = dlmemalign(alignment, bytes);
    if(gLogAlloc) log("%s(%u, %u) = %p", "dlmemalign", alignment, bytes, ptr);
    return ptr;
}
/**
 * Performs a page-aligned allocation from the heap. The size is NOT rounded up to a multiple of
 * a page size, however.
 */
void *Heap::valloc(const size_t bytes) {
    auto ptr = dlvalloc(bytes);
    if(gLogAlloc) log("%s(%u) = %p", "dlvalloc", bytes, ptr);
    return ptr;
}
/**
 * Performs a page-aligned allocation from the heap. The size is rounded up to the nearest multiple
 * of a page size.
 */
void *Heap::pvalloc(const size_t bytes) {
    auto ptr = dlpvalloc(bytes);
    if(gLogAlloc) log("%s(%u) = %p", "dlpvalloc", bytes, ptr);
    return ptr;
}


/*
 * Allocates some items and ensures the memory is zeroed.
 */
void *Heap::calloc(const size_t nItems, const size_t nBytes) {
    auto ptr = dlcalloc(nItems, nBytes);
    if(gLogAlloc) log("%s(%u, %u) = %p", "dlcalloc", nItems, nBytes, ptr);
    return ptr;
}
/**
 * Resizes an existing allocation.
 */
void *Heap::realloc(void *ptr, const size_t bytes) {
    auto outPtr = dlrealloc(ptr, bytes);
    if(gLogAlloc) log("%s(%p, %u) = %p", "dlrealloc", ptr, bytes, outPtr);
    return outPtr;
}
/**
 * Frees a previous heap allocation.
 */
void Heap::free(void *ptr) {
    if(gLogAlloc) log("%s(%p)", "dlfree", ptr);
    dlfree(ptr);
}

/**
 * Given an array of pointers, release them all and set each one to NULL.
 */
void Heap::freeBulk(void **ptrs, const size_t numPtr) {
    const auto out = dlbulk_free(ptrs, numPtr);
    if(gLogAlloc) log("%s(%p, %u) = %p", "dlbulk_free", ptrs, numPtr, out);
}



/**
 * Creates a new anonymous memory backed mapping inside the heap memory region.
 *
 * We cannot allocate any memory here, so we have to manually acquire the required pages of
 * physical memory and yeet them directly into the page tables. In the future, this should be
 * optimized to fault pages in later.
 *
 * TODO: We really need some sort of scratch space to store physical pages we allocate into; the
 * stack worked for small allocations but this can quickly get out of control. We can't use the
 * heap to allocate memory - do we maybe allocate a page to store this temporary array and fail any
 * allocations that require more than the number of page addresses we can fit? Are there any legit
 * use cases for such large kernel heap allocations?
 */
void *Heap::fakeMmap(const size_t len) {
    using namespace vm;

    void *ret = nullptr;
    int err;

    auto vm = Map::kern();

    // allocate the required number of pages
    const auto pageSz = arch_page_size();
    size_t numPages = len / pageSz;
    if(len % pageSz) {
        numPages++;
    }

    SPIN_LOCK_GUARD(gHeapLock);

    // ensure we've sufficient VM space
    if((gVmBase + (numPages * pageSz)) >= kHeapEnd) {
        // TODO: wrap back around and find a page?
        return nullptr;
    }

    // then perform the allocation
    for(size_t i = 0; i < numPages; i++) {
        // get the physical page
        auto page = mem::PhysicalAllocator::alloc();
        if(!page) {
            if(gLogMmap) log("failed to allocate phys page for heap");
            goto fail;
        }
        //pages[i] = page;

        // map it
        const auto virt = gVmBase + (i * pageSz);
        err = vm->add(page, pageSz, virt, MapMode::kKernelRW);
        if(err) {
            if(gLogMmap) log("failed to add heap mapping (at $%p): %d", virt, err);
            goto fail;
        }

        // XXX: is memory zeroed by default?
        memset(reinterpret_cast<void *>(virt), 0, pageSz);
    }

    // update the VM pointer
    ret = reinterpret_cast<void *>(gVmBase);
    gVmBase += (numPages * pageSz);

    if(gLogMmap) log("fakeMmap(%u) = %p", len, ret);
    return ret;

fail:;
    // failure case: release all physical pages and remove VM mapping
    for(size_t i = 0; i < numPages; i++) {
        uint64_t phys;
        const uintptr_t virt = (gVmBase + (i * pageSz));

        // read the page table entry
        err = vm->get(virt, phys);
        if(err == 1) continue; // no mapping
        REQUIRE(err >= 0, "failed to %s (%d)", "read page table", err);

        // there exists a mapping, so unmap and remove it
        err = vm->remove(virt, pageSz);
        REQUIRE(!err, "failed to %s (%d)", "unmap page", err);

        phys &= ~pageSz; // XXX: is this needed?
        mem::PhysicalAllocator::free(phys);
    }

    if(gLogMmap) log("fakeMmap(%u) failed", len);
    return nullptr;
}

/**
 * Releases a region of heap memory that was previously allocated. The base/length pair may span
 * more than one allocation.
 *
 * @return 0 on success, or -1 if something went wrong
 */
int Heap::fakeMunmap(const void *base, const size_t len) {
    int err = -1;
    auto vm = vm::Map::kern();
    size_t freed = 0;

    // basic error checking
    uintptr_t addr = reinterpret_cast<uintptr_t>(base);
    if((addr + len) >= kHeapEnd || addr < kHeapStart) {
        return -1;
    }

    // determine the number of pages to unmap
    const size_t pageSz = arch_page_size();
    size_t numPages = len / pageSz;
    if(len % pageSz) {
        numPages++;
    }
    addr &= ~pageSz;

    // unmap the physical pages and release them
    SPIN_LOCK_GUARD(gHeapLock);

    for(size_t i = 0; i < numPages; i++) {
        uint64_t phys;
        const uintptr_t virt = (addr + (i * pageSz));

        // read the page table entry
        err = vm->get(virt, phys);
        if(err == 1) continue; // no mapping
        else if(err < 0) goto beach; // error

        // there exists a mapping, so unmap and remove it
        err = vm->remove(virt, pageSz);
        if(err) goto beach;

        phys &= ~pageSz; // XXX: is this needed?
        mem::PhysicalAllocator::free(phys);
        freed++;
    }

    // if we get here, all pages were cleaned up
    err = 0;

beach:;
    // done
    if(gLogMmap) log("munmap(%p, %u) = %d (freed %llu)", base, len, err, freed);
    return err;
}
