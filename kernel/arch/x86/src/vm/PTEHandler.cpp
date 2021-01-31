#include "PTEHandler.h"

#include <mem/PhysicalAllocator.h>
#include <log.h>
#include <string.h>

using namespace arch::vm;



/**
 * Allocates some physical memory structures we require.
 *
 * We'll start by allocating the 4-entry (32 byte) page directory pointer table, followed by the
 * four page directories it points to. That's enough to get us set up to manipulate the rest of
 * the tables as we need, by allocating the third level page tables dynamically.
 *
 * In order to later allow access to the page tables with virtual memory enabled, we use the last
 * 4 entries (8M) at the very top of the virtual address space (0xFF800000 - 0xFFFFFFFF) to
 * recursively map the PDTs. The PDPT is essentially static so we can forget it exists.
 *
 * TODO: we waste _a lot_ of physical memory; we get one page for the 32-byte PDPT. fix this?
 */
PTEHandler::PTEHandler() {
    // allocate each of the PDTs
    for(size_t i = 0; i < 4; i++) {
        this->pdtPhys[i] = mem::PhysicalAllocator::alloc();
        // TODO: correctly set this ptr to a virtual address
        this->pdt[i] = (uint64_t *) this->pdtPhys[i];
        memset(this->pdt[i], 0, 4096);
    }

    // self-map the PDTs in the last 4 entries of the kernel PDT
    this->pdt[3][511] = this->pdtPhys[3] | 3;
    this->pdt[3][510] = this->pdtPhys[2] | 3;
    this->pdt[3][509] = this->pdtPhys[1] | 3;
    this->pdt[3][508] = this->pdtPhys[0] | 3;

    // set up PDPT
    this->pdptPhys = mem::PhysicalAllocator::alloc();

    // TODO: correctly set this ptr to a virtual address
    this->pdpt = (uint64_t *) this->pdptPhys;
    memset(this->pdpt, 0, 4096);

    for(size_t i = 0; i < 4; i++) {
        this->pdpt[i] = this->pdtPhys[i] | 0x1;
    }

    // also map the PDPT into virtual memory (TODO: is this necessary?)
    this->pdt[3][507] = this->pdptPhys | 3;
}

/**
 * Release all physical memory we allocated for page directories, tables, etc.
 *
 * @note You should not delete a page table that is currently mapped.
 */
PTEHandler::~PTEHandler() {
    // release any third level page tables (TODO)

    // release all PDTs and the PDPT
    for(size_t i = 0; i < 4; i++) {
        if(this->pdtPhys[i]) {
            mem::PhysicalAllocator::free(this->pdtPhys[i]);
        }
    }

    mem::PhysicalAllocator::free(this->pdptPhys);
}

/**
 * Updates the processor's translation table register to use our translation tables.
 */
void PTEHandler::activate() {
    // set bit 5 in CR4 to activate PAE
    asm volatile ("movl %%cr4, %%eax; bts $5, %%eax; movl %%eax, %%cr4" ::: "eax");

    // set the PDPT
    asm volatile ("movl %0, %%cr3" :: "r" (this->pdptPhys));
}
