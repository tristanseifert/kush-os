#include "PhysRegion.h"

#include <vm/Map.h>

#include <new>
#include <stdint.h>
#include <string.h>

#include <arch.h>
#include <log.h>

using namespace mem;

bool PhysRegion::gLogInit = false;
bool PhysRegion::gLogAlloc = false;
bool PhysRegion::gLogFree = false;
bool PhysRegion::gLogSplits = false;
bool PhysRegion::gLogFixups = false;
bool PhysRegion::gLogSlabHeaderFree = false;

/**
 * Return index of the most significant bit set in the given value.
 */
static inline uint64_t GetMsb64(uint64_t n) {
#if defined(__amd64__)
    uint64_t msb;
    asm("bsrq %1,%0" : "=r"(msb) : "r"(n));
    return msb;
#else
#error Implement for current arch
#endif
}

/**
 * Returns a power of two that's at least as large enough as to fit `x`
 */
static inline uint64_t GetNextPow2(uint64_t x) {
    return x == 1 ? 1 : 1<<(64-__builtin_clzl(x-1));
}

/**
 * Determines whether a given physical memory range is suitable for adding to the physical page
 * allocator.
 *
 * Currently, this just requires it's at least as large as one block in the maximum order, plus a
 * small fudge factor to allow for metadata to be placed. This means that blocks smaller than 4M
 * of physical memory will simply be ignored; hopefully, real systems have reasonably contiguous
 * memory maps.
 */
bool PhysRegion::CanAllocate(const uint64_t base, const uint64_t len) {
    const auto pageSz = arch_page_size();

    size_t bytesRequired = (pageSz * (1 << (kMaxOrder - 1)));
    bytesRequired += (64 * 1024);
    return (len > bytesRequired);
}


/**
 * Initializes a new physical allocation region.
 *
 * We'll set up structures for free lists of each order (up to the maximum order, or the largest
 * order that will fit in the region entirely) and allocate their page bitmaps.
 *
 * XXX: The allocation logic probably will have weird edge cases for regions smaller than the max
 * order, or those that are right on the boundaries. It's best to probably just ignore such
 * region for now.
 */
PhysRegion::PhysRegion(const uint64_t _base, const uint64_t _length) : base(_base),
    length(_length) {
    const auto pageSz = arch_page_size();
    const auto pageSzBits = GetMsb64(pageSz);

    // calculate total number of pages and max order
    const auto pages = this->length / pageSz;
    size_t usableLength = (pages * pageSz);

    size_t maxOrder = kMaxOrder;

    if(gLogInit) log("max order for region at $%p (len %u bytes/%u pages): %u", this->base,
            this->length, pages, maxOrder);

    /*
     * Allocate orders and their bitmaps. We allocate these from the END of the region. It's
     * possible that the space used by the bitmap will reduce the actual number of blocks of a
     * larger order and result in more broken up small blocks, so each bitmap is allocated with an
     * extra 8 bytes of trailing memory.
     */
    // first, figure out how much bitmap is required for each order
    size_t bitmapUsed = 0;
    for(size_t i = 0; i < maxOrder; i++) {
        // figure out the size of the order (in pages) and bitmap size
        const auto numBlocks = pages / (1 << i);
        if(!numBlocks) break;

        auto bitmapBytes = ((numBlocks + 8 - 1) / 8) + 8 /* overflow */;
        bitmapBytes = ((bitmapBytes + 8 - 1) / 8) * 8;

        // set up the order
        auto &o = this->orders[i];
        o.numBlocks = numBlocks;
        o.order = i;

        // "allocate" the bitmap (e.g. store the offset)
        if(i != (maxOrder - 1)) {
            o.bitmap = reinterpret_cast<uint8_t *>(bitmapUsed);
            o.bitmapSize = bitmapBytes;

            bitmapUsed += bitmapBytes;
        }
    }

    // subtract space used by bitmap
    bitmapUsed = ((bitmapUsed + pageSz - 1) / pageSz) * pageSz;
    const uintptr_t bitmapBase = this->base + this->length - bitmapUsed;

    this->bitmapOff = bitmapBase - this->base;
    this->bitmapSize = bitmapUsed;

    usableLength -= bitmapUsed;

    // convert bitmap offsets to pointers (using the physical aperture)
    for(size_t i = 0; i < maxOrder; i++) {
        auto &o = this->orders[i];

        if(o.numBlocks && o.bitmapSize) {
            o.bitmap += bitmapBase;
            o.bitmap += kEarlyPhysIdentityMap; // XXX: this should not be fixed here!
        }

        if(gLogInit) log("order %2u: %6u blocks, %5u bitmap bytes at $%p", i, o.numBlocks,
                o.bitmapSize, o.bitmap);
        memset(o.bitmap, 0, o.bitmapSize);
    }

    /**
     * Determine how much memory to reserve for the initial page structure slab, and set up the
     * allocator with this initial slab.
     */
    // total number of large blocks
    size_t numInitialBlocks = this->orders[(maxOrder - 1)].numBlocks;
    // we'll likely need some extra ones to cover the entire range
    numInitialBlocks += kMaxOrder;
    // to allow splitting later
    numInitialBlocks += kInitialBlocks;

    // allocate the initial block
    const auto initialBlockBytes = (sizeof(Block) * numInitialBlocks) + sizeof(SlabHeader);
    size_t initialBlockSize = (initialBlockBytes % pageSz) ? 
        ((initialBlockBytes + pageSz - 1) / pageSz) * pageSz : initialBlockBytes;

    usableLength -= initialBlockSize;

    // allocate a slab
    const uintptr_t slabBase = bitmapBase - initialBlockSize;
    auto slabBasePtr = reinterpret_cast<void *>(slabBase + kEarlyPhysIdentityMap);
    auto slab = new(slabBasePtr) SlabHeader(slabBase, initialBlockSize);

    this->blockSlab = slab;
    if(gLogInit) log("initial slab allocation: %u (%u blocks) at $%p ($%p)", initialBlockSize,
            numInitialBlocks, slabBase, slab);

    /**
     * Once the orders and allocator are allocated, set up free page structures for all of the
     * remaining memory.
     *
     * We'll iterate over the entire range of memory, until the amount remaining is less than can
     * be represented in the smallest order block.
     */
    size_t toAllocate = usableLength, allocated = 0;

    while(toAllocate >= pageSz) {
        const auto blockAddr = this->base + allocated;

        // find the largest order to fit this block
        size_t order = GetMsb64(toAllocate) - pageSzBits;
        if(order >= kMaxOrder) {
            order = kMaxOrder-1;
        }

testAlign:;
        size_t blockSz = pageSz * (1 << order);

        // will this block size produce unaligned blocks?
        if(blockAddr % blockSz) {
            REQUIRE(order, "cannot perform allocation: unaligned (at $%p)", blockAddr);
            order--;

            goto testAlign;
        }

        // allocate free list entry
        auto entry = this->allocBlockStruct(false);
        REQUIRE(entry, "failed to allocate block");

        entry->setAddress(allocated);
        entry->setOrder(order);

        // add it into the free list
        this->orders[order].freeBlock(entry, false, false);

        // advance to next
        // log("block %9u to alloc %12u order %2u at $%p", blockSz, toAllocate, order, blockAddr);
        toAllocate -= blockSz;
        allocated += blockSz;
    }

    this->allocatable = allocated;
    if(gLogInit) log("total allocatable: %u bytes (%u bytes overhead) $%p - $%p", toAllocate,
            this->length - toAllocate, this->base, this->base + this->allocatable);
}

/**
 * Remaps structures into the appropriate virtual memory addresses.
 *
 * This is a little tricky since we fix up the linked lists of all allocated blocks. This works by
 * converting the address inside into an offset relative to the slab, then back into a virtual
 * address. So, this will break if linked list elements are allocated from slabs other than this
 * one.
 *
 * @param base Virtual address at which the next VM mapping for the physical region shall be made
 * @param len Length of the VM region provided for the phys region to use
 * @return Number of bytes of virtual address space used (must be page aligned)
 */
uintptr_t PhysRegion::vmAvailable(const uintptr_t base, const size_t len) {
    int err;
    size_t used = 0;

    auto vm = vm::Map::kern();
    const auto pageSz = arch_page_size();

    if(gLogFixups) {
        log("fixups for %p start (%p %u)", this, base, len);
    }

    // map the bitmap region
    auto bitmapVmBase = base;
    auto bitmapVmSize = this->bitmapSize;
    auto bitmapPhys = (this->base + this->bitmapOff);

    if(bitmapSize % pageSz) {
        bitmapSize = ((bitmapSize + pageSz - 1) / pageSz) * pageSz;
    }

    if(gLogFixups) log("map bitmap: %p -> %p (len $%x)", bitmapPhys, bitmapVmBase, bitmapSize);

    err = vm->add(bitmapPhys, bitmapVmSize, bitmapVmBase, vm::MapMode::kKernelRW);
    REQUIRE(!err, "failed to map phys region %p %s: %d", this, "bitmap", err);

    used += this->bitmapSize;

    // map the allocated block slab (XXX: assumes block slabs are page sized)
    auto slabTemp = this->blockSlab;
    const auto oldSlabBase = slabTemp->vmBase;

    const auto slabPhys = reinterpret_cast<uintptr_t>(slabTemp->vmBase) - kEarlyPhysIdentityMap;
    const auto slabVirt = (base + used);
    if(gLogFixups) log("fixup %s %p -> %p", "slab", slabPhys, slabVirt);

    err = vm->add(slabPhys, slabTemp->length, slabVirt, vm::MapMode::kKernelRW);
    REQUIRE(!err, "failed to map phys region %p %s: %d", this, "slab", err);

    // update slab's vm address and ptr
    const auto slabBaseOff = reinterpret_cast<uintptr_t>(this->blockSlab) - slabPhys;

    used += slabTemp->length;

    REQUIRE(!slabTemp->next, "can only update one slab");

    // update the next pointers of any allocated blocks
    for(size_t i = 0; i < this->blockSlab->numBlocks; i++) {
        auto &block = this->blockSlab->getStorage()[i];
        if(!block.next) continue;

        // convert the next ptr
        const auto nextOff = reinterpret_cast<uintptr_t>(block.next) - oldSlabBase;
        const auto nextAddr = nextOff + slabVirt;

        if(gLogFixups) log("fixup %s %p -> %p", "block next ptr", block.next, nextAddr);

        block.next = reinterpret_cast<Block *>(nextAddr);
    }

    // apply fixups for any allocated orders
    for(size_t i = 0; i < kMaxOrder; i++) {
        auto &order = this->orders[i];

        // skip unallocated orders
        if(!order.numBlocks) {
            break;
        }

        // update allocations
        used += order.vmAvailable(*this, slabVirt, bitmapVmBase, oldSlabBase);
    }

    // update the slab pointer
    this->blockSlab = reinterpret_cast<SlabHeader *>(slabVirt + slabBaseOff);
    slabTemp->vmBase = slabVirt;

    // done
    this->vmBase = (base + used);
    this->vmLength = (len - used);

    if(gLogFixups) log("fixups for %p done", this);

    return used;
}

/**
 * Attempts to allocate a block of the given number of pages.
 */
uint64_t PhysRegion::alloc(const size_t numPages) {
    // bail if zero or more than the max order
    if(!numPages || numPages > (1 << (kMaxOrder - 1))) {
        return 0;
    }

    // convert to order
    const auto pages = GetNextPow2(numPages);
    const auto order = GetMsb64(pages);

    // allocate from this order
    return this->allocOrder(order);
}

/**
 * Allocates a block from the given order.
 *
 * @param order Zero-based order to allocate from
 */
uint64_t PhysRegion::allocOrder(const size_t order) {
    // XXX: see if we can grow the allocation
    const bool canGrowSlab = false;

    // see if the request can be satisfied with a free block in this order
    auto block = this->orders[order].allocBlock();
    if(block) {
        const auto blockAddr = block->getAddress();
        this->freeBlockStruct(block);

        // return its physical address
        const auto addr = (this->base + blockAddr);
        if(gLogAlloc) {
            log("alloc page order %2u from free list: base $%p (block $%p)", order, addr, blockAddr);
        }
        return addr;
    }

    // no free blocks; find the next largest free block
    for(size_t i = (order + 1); i < kMaxOrder; i++) {
        // get the first block out of this order
        block = this->orders[i].allocBlock();
        if(!block) continue;

        if(gLogSplits) log("splitting block %p (order %2u) for alloc order %2u", block,
                block->getOrder(), order);

        goto beach;
    }

    // if we get here, no blocks are free in this region
    return 0;

beach:;
    // split the found block until it's the right size
    while(block->getOrder() > order) {
        auto &fromOrder = this->orders[block->getOrder()];

        block = fromOrder.split(*this, block, canGrowSlab);
        if(!block) {
            // TODO: more detailed error handliong
            return 0;
        }
    }

    // block now contains a block of the right order. return its size
    REQUIRE(block->getOrder() == order, "splitting blocks failed: expected order %2u, got %2u",
            order, block->getOrder());

    const auto blockAddr = block->getAddress();
    this->freeBlockStruct(block);

    const auto addr = (this->base + blockAddr);
    if(gLogAlloc) {
        log("alloc page order %2u from split: base $%p", order, addr);
    }

    return addr;
}

/**
 * Attempts to allocate a block from the order's free list. If successful, the free bitmap is
 * updated.
 *
 * @return Free block, or `nullptr` if no free blocks available
 */
PhysRegion::Block *PhysRegion::Order::allocBlock() {
    // bail if free list empty
    if(!this->free) return nullptr;

    // get the head item and update our free ptr
    auto block = this->free;
    this->free = block->next;

    // mark it as allocated and return
    this->setBit(block);
    return block;
}

/**
 * Allocates a block from this order (n), then splits it into two. The first half of the block (of
 * order n-1) is returned as an independent block, while the second half is added to the
 * appropriate order's free list.
 *
 * The alloc bitmap is updated such that the returned block is marked as allocated.
 *
 * @param region Phys region that owns all of the block structs
 * @param block An allocated block of order n to split. Its block struct will be freed.
 * @param canGrow Whether block struct allocation is allowed to allocate memory
 *
 * @return Split block half, or `nullptr` on error
 *
 * TODO: This definitely leaks memory on error...
 */
PhysRegion::Block *PhysRegion::Order::split(PhysRegion &region, Block *block, const bool canGrow) {
    Block *block1 = nullptr, *block2 = nullptr;

    // calculate base addresses of new blocks and release the old one
    auto &smaller = region.orders[block->getOrder() - 1];

    const auto base1 = block->getAddress();
    const auto base2 = base1 + Order::size(smaller.order);

    if(gLogSplits) {
        log("splitting block %p (%2u) to %p %p", block, block->getOrder(), base1, base2);
    }

    // we can now get rid of the old block...
    region.freeBlockStruct(block);

    // create block 1 (allocated and returned for user use)
    block1 = region.allocBlockStruct(canGrow);
    if(!block1) {
        return nullptr;
    }
    block1->setAddress(base1);
    block1->setOrder(smaller.order);

    smaller.setBit(block);

    // create block 2 (available for allocation; inserted into free list)
    block2 = region.allocBlockStruct(canGrow);
    if(!block2) {
        // TODO: free block1, release the allocated block struct?
        return nullptr;
    }

    block2->setAddress(base2);
    block2->setOrder(smaller.order);

    smaller.freeBlock(block2, canGrow, false);

    // return the lower half of the block
    return block1;
}

/**
 * Adds a block to the order's free list. For simplicity, this will simply insert it at the head
 * of the list, so we needn't traverse the entire list to the end.
 */
void PhysRegion::Order::freeBlock(Block *block, const bool coalesce, const bool paranoid) {
    // insert into free map
    block->next = this->free;
    this->free = block;

    if(PhysRegion::gLogFree) {
        log("freeing: order %2u base $%p", block->getOrder(), block->getAddress());
    }

    // update bitmap
    if(paranoid) {
        // during initialization, we do some stuff that will trigger this check
        REQUIRE(this->testBit(block), "attempt to free block %p (order %2u base $%p) but is not alloc",
                block, block->getOrder(), block->getAddress());
    }
    this->clearBit(block);

    // TODO: test if it can be coalesced
    if(coalesce) {

    }
}



/**
 * Maps the block state bitmap into virtual memory. If there are any entries in the free list, we
 * will move these to virtual memory as well.
 *
 * @param blockVmBase Base address to add to free pointers
 * @return Number of bytes of virtual address space used (must be page aligned)
 */
uintptr_t PhysRegion::Order::vmAvailable(PhysRegion &region, const uintptr_t blockVmBase,
        const uintptr_t bitmapVm, const uintptr_t oldSlabBase) {
    // update bitmap ptr
    if(this->bitmap && this->bitmapSize) {
        // convert ptr into offset into bitmap region
        uintptr_t bitmapOff = reinterpret_cast<uintptr_t>(this->bitmap) - kEarlyPhysIdentityMap;
        bitmapOff -= region.base;
        bitmapOff -= region.bitmapOff;

        // get offset into mapped region
        const uintptr_t newBitmapAddr = bitmapVm + bitmapOff;
        auto newBitmap = reinterpret_cast<uint8_t *>(newBitmapAddr);

        //log("old bitmap %p new %p (vm %p off %x)", this->bitmap, newBitmapAddr, bitmapVm, bitmapOff);
        if(PhysRegion::gLogFixups) log("fixup %s %p -> %p", "order bitmap ptr", this->bitmap,
                newBitmap);
        this->bitmap = newBitmap;
    }

    // update pointer to the first free list item (the next pointers have been taken care of)
    if(this->free) {
        const auto freeOff = reinterpret_cast<uintptr_t>(this->free) - oldSlabBase;
        const auto freeAddr = freeOff + blockVmBase;

        if(PhysRegion::gLogFixups) log("fixup %s %p -> %p", "order free ptr", this->free, freeAddr);

        this->free = reinterpret_cast<Block *>(freeAddr);
    }

    return 0;
}

/**
 * Returns the size, in bytes, of a single block in the given order.
 */
size_t PhysRegion::Order::size(const size_t order) {
    REQUIRE(order < kMaxOrder, "invalid order: %u", order);

    const auto pageSz = arch_page_size();
    return pageSz * (1 << order);
}



/**
 * Sets up a new slab, with the given virtual base address and length. All memory in the given
 * region, which isn't used by the fixed header, is allocated to the storage array.
 *
 * @param base Virtual address of the base of the memory region. It's assumed that this object is
 * being constructed at the top of this range.
 * @param length Total size of the virtual memory region, in bytes.
 */
PhysRegion::SlabHeader::SlabHeader(const uintptr_t base, const size_t len) : vmBase(base),
    length(len) {
    // ensure base and length are page aligned
    const auto pageSz = arch_page_size();
    REQUIRE(!(base % pageSz) && !(len % pageSz), "unaligned base ($%x) or length ($%x)", base, len);

    // how many blocks can we fit in the leftover area?
    const auto leftover = len - sizeof(SlabHeader);
    auto numBlocks = leftover / sizeof(Block);

    auto bitmapSize = (numBlocks + 8 - 1) / 8; // always round up to nearest byte
    bitmapSize = (bitmapSize % 8) ? ((bitmapSize + 8 - 1) / 8) * 8 : bitmapSize; // 8 byte align

    numBlocks = (leftover - bitmapSize) / sizeof(Block);

    // set up bitmap region
    this->bitmapOff = sizeof(SlabHeader);

    auto bitmapPtr = reinterpret_cast<uint8_t *>(this->vmBase + this->bitmapOff);
    memset(bitmapPtr, 0, bitmapSize);

    // and the storage region
    this->numBlocks = numBlocks;
    this->storageOff = this->bitmapOff + bitmapSize;

    auto storagePtr = reinterpret_cast<void *>(this->vmBase + this->storageOff);
    memset(storagePtr, 0, sizeof(Block) * numBlocks);

    // mark these blocks as available
    memset(bitmapPtr, 0xFF, numBlocks / 8);
    if(numBlocks % 8) {
        for(size_t i = 0; i < numBlocks % 8; i++) {
            bitmapPtr[numBlocks / 8] |= (1 << i);
        }
    }

    if(PhysRegion::gLogInit) log("slab %p: base %p (len %u) bitmap %u bytes, %u blocks", this,
            base, len, bitmapSize, numBlocks);
}

/**
 * Allocates a new block structure.
 *
 * @param grow When set, and there are no free blocks available, we may allocate more memory for
 * another slab.
 * @return Pointer to block, or `nullptr` if failed to allocate
 */
PhysRegion::Block *PhysRegion::allocBlockStruct(const bool grow) {
    auto slab = this->blockSlab;

    while(slab) {
        // check if it can allocate
        auto block = slab->alloc();
        if(block) {
            return block;
        }

        // go to next slab
        slab = slab->next;
    }

    // no free space in any slab; allocate a new one if allowed
    if(grow) {
        // try allocating again
        return this->allocBlockStruct(false);
    }

    // failed to find a block
    return nullptr;
}

/**
 * Deallocates a block previously allocated from one of the slabs.
 */
void PhysRegion::freeBlockStruct(Block *block) {
    // iterate over all slabs
    auto slab = this->blockSlab;

    while(slab) {
        if(slab->free(block)) {
            return;
        }

        // go to next slab
        slab = slab->next;
    }

    // failed to free the block
    panic("failed to free block $%p", block);
}

/**
 * Attempts to allocate a block from this slab.
 *
 * @return Address of a newly allocated (and zeroed) block, or `nullptr` if the allocation could
 * not be satisfied from this slab.
 */
PhysRegion::Block *PhysRegion::SlabHeader::alloc() {
    // check the bitmap, 64 bits at a time
    auto bitmap = reinterpret_cast<uint64_t *>(this->vmBase + this->bitmapOff);

    for(size_t maj = 0; maj < ((this->numBlocks + 64 - 1) / 64); maj++) {
        // get index of first free block (first 1 bit)
        if(!bitmap[maj]) continue;
        const auto free = __builtin_ffsll(bitmap[maj]) - 1;

        // return the address of this block after we mark it as allocated
        const auto blockIdx = (maj * 64) + free;
        bitmap[blockIdx / 64] &= ~(1ULL << (blockIdx % 64));

        // log("allocated block %p idx %u", this->getStorage() + blockIdx, blockIdx);
        return this->getStorage() + blockIdx;
    }

    // failed to find free entry
    return nullptr;
}

/**
 * Releases a block, if it was allocated from this slab.
 *
 * @return Whether the block was contained in this slab AND was allocated
 */
bool PhysRegion::SlabHeader::free(Block *block) {
    auto bitmap = reinterpret_cast<uint64_t *>(this->vmBase + this->bitmapOff);

    // validate the address
    auto blockBase = reinterpret_cast<uintptr_t>(block);
    const uintptr_t storageBase = reinterpret_cast<uintptr_t>(this->getStorage());
    const uint64_t blockIdx = (blockBase - storageBase) / sizeof(Block);
    REQUIRE(!((blockBase - storageBase) % sizeof(Block)), "unaligned ptr: $%p", block);

    if(blockIdx >= this->numBlocks) {
        return false;
    }

    // test whether it's allocated
    const size_t bitmapIdx = (blockIdx / 64);
    const size_t bitmapBit = (blockIdx % 64);

    const uint64_t entry = bitmap[bitmapIdx];
    const uint64_t bit = (1ULL << bitmapBit);

    if(gLogSlabHeaderFree) {
        log("free block %p idx %4u bitmap %016llx %016llx %016llx %4lu %4lu", block, blockIdx, 
            entry, bit, entry & bit, bitmapIdx, bitmapBit);
    }

    if(entry & bit) {
        panic("attempt to free $%p (idx %u) but is not marked as allocated", block, blockIdx);
        return false;
    }

    // it's allocated; release its memory and clear in bitmap
    bitmap[bitmapIdx] |= bit;

    block->~Block();
    memset(block, 0, sizeof(Block));

    return true;
}
