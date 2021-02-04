#include "StackPool.h"
#include "PhysicalAllocator.h"

#include "vm/Map.h"

#include <new>
#include <arch.h>
#include <log.h>
#include <string.h>

using namespace mem;

// enable logging of stack section VM updates
#define LOG_VM_UPDATE                   0

static char gSharedBuf[sizeof(StackPool)];
StackPool *StackPool::gShared = (StackPool *) &gSharedBuf;

/**
 * Sets up the global stack pool after VM becomes available.
 */
void StackPool::init() {
    new(gShared) StackPool();
}

/**
 * Initializes the stack pool.
 */
StackPool::StackPool() {
    // mark all stacks as available
    memset(&this->freeMap, 0, sizeof(uint32_t) * (kNumStacks / 4));

    for(size_t i = 0; i < kNumStacks; i++) {
        this->freeMap[i / 32] |= (1 << (i % 32));
    }
}

/**
 * Finds a new VM region to allocate to a stack.
 *
 * The topmost page of the kStackSize region is not mapped; it's a guard page between stacks.
 */
void *StackPool::alloc() {
    int err;
    const auto pageSz = arch_page_size();
    auto m = vm::Map::kern();

    // get a lock on the free map
    SPIN_LOCK_GUARD(this->freeMapLck);

    // search the free map
    for(size_t i = 0; i < (kNumStacks / 32); i++) {
        // if no free space in this element, check next
        if(!this->freeMap[i]) continue;
        // get index of the first free (set) bit. can return 0 if no bits set but we guard above
        const size_t allocBit = __builtin_ffs(this->freeMap[i]) - 1;
        // mark it as allocated and get the index
        this->freeMap[i] &= ~(1 << allocBit);

        const auto freeIdx = (i * 32) + allocBit;

        // allocate the physical pages first
        const auto pages = kStackSize / pageSz;
        const uintptr_t start = kBaseAddr + (freeIdx * kStackSize);

        uint64_t physPages[pages] = {0};

        for(size_t i = 1; i < pages; i++) {
            // allocate physical page
            auto physPage = PhysicalAllocator::alloc();
            if(!physPage) goto failure;
            physPages[i] = physPage;
        }

        // map them
        for(size_t i = 1; i < pages; i++) {
            const auto vmAddr = start + (i * pageSz);
            err = m->add(physPages[i], pageSz, vmAddr, vm::MapMode::kKernelRW);

#if LOG_VM_UPDATE
            log("stack mapped phys %016llx to %08lx", physPages[i], vmAddr);
#endif

            if(err) {
                log("failed to map stack %016llx to %08lx: %d", physPages[i], vmAddr, err);
                goto failure;
            }
        }

        // clear the entire allocated stack region and return it
        memset((void *) (start + pageSz), 0, (kStackSize - pageSz));
        return (void *) (start + kStackSize);

failure:;
        // error: release all phys pages again
        for(size_t i = 0; i < pages; i++) {
            if(physPages[i]) {
                PhysicalAllocator::free(physPages[i]);
            }
        }
        return nullptr;
    }

    // no VM space for stacks
    return nullptr;
}

/**
 * Releases the given stack back to the system. The physical memory is deallocated.
 */
void StackPool::free(void *base) {
    int err;
    uint64_t phys;

    const auto pageSz = arch_page_size();
    const auto stackPages = kStackSize / pageSz;

    auto m = vm::Map::kern();

    // get a lock on the free map
    SPIN_LOCK_GUARD(this->freeMapLck);

    // ensure it's marked as allocated in the map
    const uintptr_t idx = ((((uintptr_t) base) - kBaseAddr) / kStackSize) - 1;
    REQUIRE(idx < kNumStacks, "stack ptr (%p) out of bounds", base);

    const auto index = idx / 32;
    const auto bit = idx % 32;

    REQUIRE((this->freeMap[index] & (1 << bit)) == 0, "can't free unallocated stack %p (%ld)",
            base, idx);

    // for each stack page, zero it, unmap it, then release the physical space
    const uintptr_t start = kBaseAddr + (idx * kStackSize);

    for(size_t i = 0; i < stackPages; i++) {
        const auto vmAddr = start + (i * pageSz);

        // get physical address (and ignore guard pages)
        err = m->get(vmAddr, phys);
        if(err == 1) continue;
        REQUIRE(!err, "failed to %s stack page: %d", "get phys addr of", err);

        // zero the page while it's already mapped
        memset((void *) vmAddr, 0, pageSz);

        // unmap the virtual address
        err = m->remove(vmAddr, pageSz);
        REQUIRE(!err, "failed to %s stack page: %d", "unmap", err);

#if LOG_VM_UPDATE
        log("stack unmapped phys %016llx to %08lx", phys, vmAddr);
#endif

        // then release the physical page
        PhysicalAllocator::free(phys);
    }

    // mark this virtual memory slot as available again
    this->freeMap[index] |= (1 << bit);
}
