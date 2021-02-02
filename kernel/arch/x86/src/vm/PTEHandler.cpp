#include "PTEHandler.h"
#include "PDPTPool.h"

#include <mem/PhysicalAllocator.h>
#include <arch.h>
#include <log.h>
#include <string.h>

using namespace arch::vm;

// log the modification of page directories
#define LOG_PDE_UPDATE          0
// log allocation of page directories
#define LOG_PDE_ALLOC           0
// log mappings
#define LOG_MAP                 0

/**
 * Allocates some physical memory structures we require.
 *
 * We'll start by allocating the 4-entry (32 byte) page directory pointer table, followed by the
 * four page directories it points to. That's enough to get us set up to manipulate the rest of
 * the tables as we need, by allocating the third level page tables dynamically.
 *
 * TODO: we waste _a lot_ of physical memory; we get one page for the 32-byte PDPT. fix this?
 */
PTEHandler::PTEHandler(::vm::IPTEHandler *_kernel) {
    auto kernelPte = reinterpret_cast<PTEHandler *>(_kernel);

    if(kernelPte) {
        this->initCopyKernel(kernelPte);
    } else {
        this->initKernel();
    }
}

/**
 * Initializes a kernel page table.
 * 
 * Note at this stage, the virtual addresses for various structures are simply set to be the same
 * as their physical addresses.
 */
void PTEHandler::initKernel() {
    // allocate each of the PDTs
    for(size_t i = 0; i < 4; i++) {
        this->pdtPhys[i] = mem::PhysicalAllocator::alloc();
        this->pdt[i] = (uint64_t *) this->pdtPhys[i];
        memset(this->pdt[i], 0, 4096);
    }

    // self-map the PDTs in the last 4 entries of the top of userspace
    this->pdt[2][511] = this->pdtPhys[3] | 0b011;
    this->pdt[2][510] = this->pdtPhys[2] | 0b011;
    this->pdt[2][509] = this->pdtPhys[1] | 0b011;
    this->pdt[2][508] = this->pdtPhys[0] | 0b011;

    // set up PDPT
/*    uintptr_t pdptVirt;
    err = PDPTPool::alloc(pdptVirt, this->pdptPhys);
    REQUIRE(!err, "failed to allocate PDPT: %d", err);
    this->pdpt = (uint64_t *) pdptVirt;*/

    this->pdptPhys = mem::PhysicalAllocator::alloc();

    this->pdpt = (uint64_t *) this->pdptPhys;
    memset(this->pdpt, 0, 4096);

    for(size_t i = 0; i < 4; i++) {
        this->pdpt[i] = this->pdtPhys[i] | 0b01;
    }

    // also map the PDPT into virtual memory (TODO: is this necessary?)
    this->pdt[2][507] = this->pdptPhys | 0b011;
}

/**
 * Initializes a user mapping table that references the given kernel page table.
 *
 * In our case, this means the top 1GB of address space is simply pointed to the kernel's page
 * directory.
 */
void PTEHandler::initCopyKernel(PTEHandler *kernel) {
    int err;

    // TODO: this is likely broken lol

    // allocate three userspace PDTs; then copy the kernel PDT
    for(size_t i = 0; i < 3; i++) {
        this->pdtPhys[i] = mem::PhysicalAllocator::alloc();
    }

    this->pdtPhys[3] = 0;
    this->pdt[3] = kernel->pdt[3];

    // self-map the PDTs in the last 4 entries of the top of userspace
    this->pdt[2][511] = kernel->pdtPhys[3] | 0b011;
    this->pdt[2][510] = this->pdtPhys[2] | 0b011;
    this->pdt[2][509] = this->pdtPhys[1] | 0b011;
    this->pdt[2][508] = this->pdtPhys[0] | 0b011;

    // set up PDPT
    uintptr_t pdptVirt;
    err = PDPTPool::alloc(pdptVirt, this->pdptPhys);
    REQUIRE(!err, "failed to allocate PDPT: %d", err);

    this->pdpt = (uint64_t *) pdptVirt;

    for(size_t i = 0; i < 3; i++) {
        this->pdpt[i] = this->pdtPhys[i] | 0b001;
    }
    this->pdpt[3] = kernel->pdtPhys[3] | 0b001;

    // also map the PDPT into virtual memory (TODO: is this necessary?)
    this->pdt[2][507] = this->pdptPhys | 3;
}

/**
 * Release all physical memory we allocated for page directories, tables, etc.
 *
 * This _will_ leak all memory for translations above 0xC0000000, but that's ok since we'll never
 * actually deallocate those; even when shutting the system down, virtual memory just stays on.
 *
 * @note You should not delete a page table that is currently mapped.
 */
PTEHandler::~PTEHandler() {
    // release any third level page tables below 0xC0000000
    for(size_t i = 0; i < (512*3); i++) {
        const uint32_t virt = (i * 0x200000);
        const uint64_t pdtValue = this->getPageDirectory(virt);

        // ignore if not present
        if(!(pdtValue & (1 << 0))) continue;
        // ignore if it's a 2M page
        else if(pdtValue & (1 << 7)) continue;

        // otherwise, get the physical address
        const auto physAddr = pdtValue & ~0xfff;
        mem::PhysicalAllocator::free(physAddr);
    }

    // release all PDTs
    for(size_t i = 0; i < 4; i++) {
        if(this->pdtPhys[i]) {
            mem::PhysicalAllocator::free(this->pdtPhys[i]);
        }
    }

    // release PDPT back to the pool, otherwise we own the page and should free it
    mem::PhysicalAllocator::free(this->pdptPhys);
}

/**
 * Updates the processor's translation table register to use our translation tables.
 */
void PTEHandler::activate() {
    // set the PDPT
    // log("switching to PDPT $%016lx", this->pdptPhys);
    asm volatile("movl %0, %%cr3" :: "r" (this->pdptPhys));
}

/**
 * Read the CR3 reg and see if it contains the address of our PDPT.
 */
const bool PTEHandler::isActive() const {
    uint32_t pdptAddr;
    asm volatile("mov %%cr3, %0" : "=r" (pdptAddr));

    return (pdptAddr == this->pdptPhys);
}



/**
 * Maps a single 4K page.
 */
int PTEHandler::mapPage(const uint64_t phys, const uintptr_t virt, const bool write,
        const bool execute, const bool global, const bool user) {
    uint64_t *table = nullptr;

    // build flags (present)
    uint64_t flags = 0b1;

    if(!execute && arch_supports_nx()) { // NX bit set if execute is not set
        flags |= (1ULL << 63);
    }
    if(user) { // allow user accesses if set
        flags |= (1 << 2);
    }
    if(write) { // allow writes if set
        flags |= (1 << 1);
    }
    if(global) { // mark as global if set
        flags |= (1 << 8);
    }

    flags |= ((uint64_t) phys);

    // special case if mapped
    if(this->isActive()) {
        // log("PDE for 0xC0100000: %016llx", this->getPageDirectory(0xC0100000));

        // see if we need to allocate the page table
        const auto pdtEntry = this->getPageDirectory(virt);
        if(!(pdtEntry & 0b1)) {
            // allocate the table
            const auto page = mem::PhysicalAllocator::alloc();
            REQUIRE(page, "failed to allocate page table");

            // flags for the pages mapped in this 2M chunk: RW and present
            uint64_t flags = 0b00000011;

            // allow for it to be userspace accessible if outside the kernel zone
            if(virt < 0xC0000000) {
                flags |= 0b100;
            }

            // yeet it into the page directory
            this->setPageDirectory(virt, page | flags);

#if LOG_PDE_ALLOC
            log("allocated page table (%p) for $%lx (pde entry $%016llx)", (void *) page, virt, page | flags);
#endif
        } else {
            // this region is mapped as a 2M page. cannot add sub-mapping
            if((pdtEntry & (1 << 7))) {
                return -1;
            }
        }

        // we allocated the page table (or ensured it's valid) so write the physical page address
        this->setPageTable(virt, flags);
    } else {
        // get the page directory and within it the index
        const auto pdptOff = (virt & 0xC0000000) >> 30;
        const auto pdeOff = (virt & 0x3FE00000) >> 21;

        auto directory = this->pdt[pdptOff];

        // allocate a page for the page table if needed
        if(!directory[pdeOff] & 0b1) {
            // present and RW allowed
            uint64_t flags = 0b00000011;

            // allow for it to be userspace accessible under the kernel zone
            if(virt < 0xC0000000) {
                flags |= 0b100;
            }

            // allocate the table
            const auto page = mem::PhysicalAllocator::alloc();
            REQUIRE(page, "failed to allocate page table");

            // get its virtual address (TODO: better)
            table = (uint64_t *) page;
            memset(table, 0, 4096);

            // write the address into the directory
            directory[pdeOff] = ((uint64_t) page) | flags;

#if LOG_PDE_ALLOC
            log("allocated page table (%p) for $%lx (pde entry $%016llx): $%p", (void *) page, virt, directory[pdeOff], (void *) table);
#endif
        }
        // otherwise, get its virtual address
        else {
            table = (uint64_t *) (directory[pdeOff] & 0xFFFFF000);

#if LOG_PDE_UPDATE
            log("page table for $%lx (pdt entry $%016llx): $%p", virt, directory[pdeOff], (void *) table);
#endif
        }

        // update the entry in the table
        const auto ptOff = (virt & 0x001FF000) >> 12;

        // TODO: check existing mapping

        // write into page table
        table[ptOff] = flags;

#if LOG_MAP
        log("mapped virt $%08lx to phys $%016llx: PDPT off %lu PDE entry %016llx off %lu PT entry %016llx off %lu", virt, phys,
                pdptOff, directory[pdeOff], pdeOff, table[ptOff], ptOff);
#endif
    }

    // successfully mapped the page
    return 0;
}

/**
 * Sets the value of the page directory. This uses the spicy special mapped zone.
 *
 * @note You can always invoke this method. For any given PTE handler, we already allocated all
 * four of the page directories.
 */
void PTEHandler::setPageDirectory(const uint32_t virt, const uint64_t value) {
    uint64_t *ptr = (uint64_t *) 0xbf600000;
    ptr[(virt >> 21)] = value;

    // invalidate the virtual address region for this page directory
    asm volatile( "invlpg (%0)" : : "b"(&ptr[(virt >> 21)]) : "memory" );
    asm volatile( "invlpg (%0)" : : "b"(&ptr[(virt >> 9)]) : "memory" );
}
/**
 * Gets the page directory entry for the given virtual address.
 */
const uint64_t PTEHandler::getPageDirectory(const uint32_t virt) {
    uint64_t *ptr = (uint64_t *) 0xbf600000;
    return ptr[(virt >> 21)];
}


/**
 * Sets the value of a page table entry.
 *
 * @note It's required to check that the page table has been allocated before.
 */
void PTEHandler::setPageTable(const uint32_t virt, const uint64_t value) {
    uint64_t *ptr = (uint64_t *) (0xBF800000 + (virt >> 9));
    *ptr = value;

    // TODO: is this always required?
    asm volatile( "invlpg (%0)" : : "b"(virt) : "memory" );
}
