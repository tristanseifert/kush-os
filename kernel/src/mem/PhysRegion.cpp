#include "PhysRegion.h"

#include <vm/Map.h>

#include <new>
#include <stdint.h>
#include <string.h>

#include <arch.h>
#include <log.h>

using namespace mem;

bool PhysRegion::gLogInit       = false;
bool PhysRegion::gLogAlloc      = false;
bool PhysRegion::gLogFree       = false;



/**
 * Determines whether a given physical memory range is suitable for adding to the physical page
 * allocator.
 *
 * The only size requirement we impose is that the region is at least two pages in length, so that
 * we have space for the PhysRegion struct (1 page) and the memory it manages (up to 96M)
 */
bool PhysRegion::CanAllocate(const uint64_t base, const uint64_t len) {
    const auto pageSz = arch_page_size();
    return (len > (pageSz * kMinPages));
}


/**
 * Initializes a new physical allocation region that encompasses the entirety of the given range of
 * physical memory.
 *
 * This only sets up the first `PhysRegion` object to cover the initial 96M of memory. If there is
 * any additional memory in this region, additional blocks are allocated once the virtual memory
 * system has been fully initialized.
 */
PhysRegion::PhysRegion(const uint64_t _base, const uint64_t _length) : base(_base),
    length(_length) {
    // calculate the total pages in this region
    const auto pageSz = arch_page_size();
    auto pages = this->length / pageSz;

    REQUIRE(pages >= kMinPages, "region at $%p too small: %lu bytes", _base, _length);

    if(pages > kMaxPages) {
        pages = kMaxPages;
    }

    this->commonInit(pages);
}

/**
 * Allocates a physical region that is a "child" of another region. It takes its base/length from
 * the given parent.
 */
PhysRegion::PhysRegion(PhysRegion *_parent, const uint64_t offset) : vmAvailable(true),
    parent(_parent) {
    const auto pageSz = arch_page_size();

    // calculate the appropriate base address and lengths
    REQUIRE(!(offset % pageSz), "%s must be page aligned (is %x)", "region offset", offset);
    this->base = _parent->base + offset;

    this->length = _parent->length - offset;

    // then figure out how many pages we represent
    auto pages = this->length / pageSz;
    REQUIRE(pages >= kMinPages, "region at $%p too small: %lu bytes", this->base, this->length);

    if(pages > kMaxPages) {
        pages = kMaxPages;
    }

    this->commonInit(pages);
}

/**
 * Performs shared initialization; this consists of setting up the bitmap.
 */
void PhysRegion::commonInit(const size_t numPages) {
    REQUIRE(numPages <= kMaxPages, "invalid number of pages: %lu", numPages);

    const auto pageSz = arch_page_size();
    this->allocatable = numPages * pageSz;
    this->bitmapMax = (numPages - 1);

    if(gLogInit) {
        log("PhysRegion: init: base $%p length $%x", this->base, this->allocatable);
    }

    // mark the allocatable ones as free (set)
    for(size_t i = 0; i < this->bitmapMax; i++) {
        this->bitmap[i / 64] |= (1ULL << (i % 64));
    }
}


/**
 * Invoked once virtual memory and the heap are available.
 *
 * If this physical region has additional memory beyond the first 24k pages represented by this
 * object, we'll create them and build them into a linked list.
 */
void PhysRegion::initNextIfNeeded() {
    const auto pageSz = arch_page_size();

    // set up some general state
    this->vmAvailable = true;

    // determine how much we can allocate and whether it's sufficient for a new block
    auto offset = this->allocatable;
    REQUIRE(!(offset % pageSz), "%s length must be page aligned (is %x)", "allocatable", offset);

    auto remaining = this->length - this->allocatable;
    REQUIRE(!(offset % pageSz), "%s length must be page aligned (is %x)", "remaining", remaining);

    auto nextBase = this->base + offset;

    if(!remaining || !CanAllocate(nextBase, remaining)) {
        return;
    }

    // we should allocate a new block :D
    PhysRegion *region = this;

    while(CanAllocate(nextBase, remaining)) {
        // create the new region
        auto child = new PhysRegion(this, offset);

        // set up pointers and advance
        region->next = child;
        region = child;

        // recalculate base/remaining
        offset += child->allocatable;
        remaining -= child->allocatable;
        nextBase = this->base + offset;
    }
}



/**
 * Allocates a single page of physical memory.
 *
 * Currently, we iterate the entire bitmap every time.
 */
uint64_t PhysRegion::alloc() {
    SPIN_LOCK_GUARD(this->lock);

    const size_t maxIdx = ((this->bitmapMax + 63) / 64);
    for(size_t i = 0; i < maxIdx; i++) {
        // bail if all 64 pages are allocated
        if(!this->bitmap[i]) continue;

        // find the page offset of the allocated page
        const size_t bitOff = __builtin_ffsl(this->bitmap[i]) - 1;
        REQUIRE(bitOff <= 63, "invalid bit offset %lu", bitOff);

        const size_t pageOff = (i * 64) + bitOff;

        // mark as allocated, and return its physical address
        this->bitmap[i] &= ~(1ULL << bitOff);
        const uint64_t pageAddr = this->base + (pageOff * arch_page_size());

        if(gLogAlloc) [[unlikely]] {
            log("PhysRegion: %s: page %p (off %lu)", "alloc", pageAddr, pageOff);
        }

        // zero the page
        this->zero(pageAddr);

        return pageAddr;
    }

    // no memory available
    return 0;
}

/**
 * Frees all physical pages in the given list.
 *
 * @note If a physical address in the list is not contained in this region, it is ignored.
 *
 * @param pages Array of physical addresses of pages to free
 * @param numPages Total number of pages to free
 *
 * @return Actual number of pages freed
 */
int PhysRegion::free(const uint64_t *pages, const size_t numPages) {
    SPIN_LOCK_GUARD(this->lock);

    const auto pageSz = arch_page_size();
    int freed = 0;

    for(size_t i = 0; i < numPages; i++) {
        // ensure this region contains the given address
        const uint64_t page = pages[i];
        if(!this->checkAddress(page)) continue;

        // free this page (set its bit)
        const size_t pageOff = (page - this->base) / pageSz;
        REQUIRE(pageOff < this->bitmapMax, "attempting to free invalid page %p (off %lu)", page,
                pageOff);

        this->bitmap[pageOff / 64] |= (1ULL << (pageOff % 64));
        freed++;

        if(gLogFree) [[unlikely]] {
            log("PhysRegion: %s: page %p (off %lu)", "free", page, pageOff);
        }
    }

    return freed;
}

/**
 * Zero fills a page.
 */
void PhysRegion::zero(const uint64_t physAddr) {
    if(this->vmAvailable) [[likely]] {
        auto ptr = reinterpret_cast<void *>(kPhysIdentityMap + physAddr);
        memset(ptr, 0, arch_page_size());
    }
}



/**
 * Sums the number of available allocatable bytes in this region and all subsequent children.
 */
uint64_t PhysRegion::getAvailableBytes() const {
    uint64_t sum{this->allocatable};
    auto next = this->next;

    while(next) {
        sum += next->allocatable;
        next = next->next;
    }

    return sum;
}
