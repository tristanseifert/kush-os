#include "PDPTPool.h"

#include <mem/AnonPool.h>

#include <log.h>
#include <new>

using namespace arch::vm;

/// whether allocations/deallocations of PDPTs are logged
#define LOG_ALLOC                       0

/// shared pool instance
static char gSharedBuf[sizeof(PDPTPool)];
PDPTPool *PDPTPool::gShared = (PDPTPool *) &gSharedBuf;

/**
 * Runs the constructor for the statically allocated pool.
 */
void PDPTPool::init() {
    new(gShared) PDPTPool();
}

/**
 * Initializes the PDPT pool. We'll allocate our first page of pointer directories now.
 */
PDPTPool::PDPTPool() {
    // set up the initial page
    for(size_t i = 0; i < kMaxPages; i++) {
        this->pages[i] = nullptr;
    }
    this->allocPage();
}

/**
 * Releases all physical memory we've allocated.
 *
 * XXX: This probably will never get called.
 */
PDPTPool::~PDPTPool() {
    // release the physical memory of each page
    for(size_t i = 0; i < kMaxPages; i++) {
        if(!this->pages[i]) continue;
        mem::AnonPool::freePage(this->pages[i]);

        this->pages[i]->~Page();
    }
}



/**
 * Allocates a new PDPT from the pool. We'll search all allocated pages, starting with the one at
 * the free pointer, wrapping around if need be. If no pages contain free PDPTs, we'll allocate
 * another page.
 */
int PDPTPool::getPdpt(uintptr_t &virt, uintptr_t &phys) {
again:;
    // search for free page
    for(size_t i = 0; i < kMaxPages; i++) {
        if(!this->pages[i]) continue;
        const auto &page = this->pages[i];

        if(page->hasVacancies()) {
            return page->alloc(virt, phys) ? 0 : -2;
        }
    }

    // allocate new page
    if(this->allocPage()) {
        goto again;
    }

    // if we get here, we couldn't allocate a PDPT
    return -1;
}

/**
 * Allocates an additional page for PDPT storage.
 */
bool PDPTPool::allocPage() {
    // ensure we don't allocate more pages than we're allowed to
    for(size_t i = 0; i < kMaxPages; i++) {
        if(!this->pages[i]) goto beach;
    }
    return false;
beach:;

    // get a page of anonymous memory
    uint64_t phys;
    void *mem = mem::AnonPool::allocPage(phys);
    REQUIRE(mem, "failed to alloc page for PDPT pool");
    REQUIRE(!(phys & 0xFFFFFFFF00000000), "PDPT page must be in low 4G of physmem");

    /// XXX: uhhhhhh lmao
    Page *page = reinterpret_cast<Page *>(mem);

    // initialize it
    new(page) Page((uintptr_t) phys);

    for(size_t i = 0; i < kMaxPages; i++) {
        if(!this->pages[i]) {
            this->pages[i] = page;
            return true;
        }
    }

    // we should never get here!
    return false;
}

/**
 * Gets the virtual and physical address of the next free PDPT in the page.
 */
bool PDPTPool::Page::alloc(uintptr_t &virt, uintptr_t &phys) {
    // check 32 bits at a time
    for(size_t i = 0; i < 4; i++) {
        // ignore if no vacancies in this block of 32
        if(!this->freeMap[i]) continue;

        // find the bit to allocate and mark it as allocated
        const size_t allocBit = __builtin_ffs(this->freeMap[i]) - 1;
        this->freeMap[i] &= ~(1 << allocBit);

        const auto pdptIdx = (i * 32) + allocBit;

        // clear this PDPT and output its address
        memset(&this->data[pdptIdx], 0, 32);

        phys = this->physAddr + ((uintptr_t) &this->data[pdptIdx] - (uintptr_t) this);
        virt = (uintptr_t) &this->data[pdptIdx];

#ifndef LOG_ALLOC
        log("allocated: idx %d (%d, %d) %08lx %08lx", pdptIdx, i, allocBit, phys, virt);
#endif

        return true;
    }

    // no vacancies if we get here...
    return false;
}



/**
 * Marks the given PDPT as available.
 */
void PDPTPool::freePdpt(const uintptr_t phys) {
    // find the page
    const auto pageAddr = phys & ~0xFFF;

    for(size_t i = 0; i < kMaxPages; i++) {
        if(!this->pages[i]) continue;
        else if(this->pages[i]->physAddr != pageAddr) continue;

        return this->pages[i]->free(phys);
    }

    // if we get here, the address doesn't belong to the pool
    panic("attempt to free PDPT %08x; but not in pool", phys);
}

/**
 * Marks a PDPT as available based on its physical address.
 */
void PDPTPool::Page::free(const uintptr_t phys) {
    // convert physical address into an index
    auto idx = phys - this->physAddr;
    idx -= offsetof(Page, data[0][0]);
    idx /= 32;

    // mark it as available
    const auto index = idx / 32;
    const auto bit = idx % 32;

#ifndef LOG_ALLOC
    log("freeing: phys %08x idx %d (%d, %d)", phys, idx, index, bit);
#endif

    this->freeMap[index] |= (1 << bit);
}

/**
 * Checks if the physical address for this PDPT falls within that of any of the allocated pages.
 */
bool PDPTPool::isPdptFromPool(const uintptr_t phys) {
    const auto pageAddr = phys & ~0xFFF;

    for(size_t i = 0; i < kMaxPages; i++) {
        if(!this->pages[i]) continue;
        // check if the page physical addresses are the same
        if(this->pages[i]->physAddr == pageAddr) return true;
    }

    // no page matches this physical range
    return false;
}
