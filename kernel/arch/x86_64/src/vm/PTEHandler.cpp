#include "PTEHandler.h"

#include <vm/Map.h>
#include <mem/PhysicalAllocator.h>
#include <arch.h>
#include <log.h>
#include <string.h>

using namespace arch::vm;

/// Pointer to the kernel PTE
arch::vm::PTEHandler *gArchKernelPte = nullptr;


/**
 * Allocates some physical memory structures we require.
 *
 * We'll start by allocating the 4-entry (32 byte) page directory pointer table, followed by the
 * four page directories it points to. That's enough to get us set up to manipulate the rest of
 * the tables as we need, by allocating the third level page tables dynamically.
 *
 * TODO: we waste _a lot_ of physical memory; we get one page for the 32-byte PDPT. fix this?
 */
PTEHandler::PTEHandler(::vm::IPTEHandler *_parent) : IPTEHandler(_parent) {
    auto kernelPte = reinterpret_cast<PTEHandler *>(_parent);
    this->parent = kernelPte;

    if(kernelPte) {
        // TODO: reference all the kernel tables for above 0x80000000'00000000
    } else {
        this->initKernel();
    }
}

/**
 * Initializes the page table handler for the kernel mapping.
 *
 * This is the first mapping we create, and all further mappings will copy the top 256 entries of
 * our PML4. What this means is that we need to allocate all the page directories we'll ever need
 * ahead of time now, so the PML4 contains pointers to them.
 *
 * Note that we access the returned physical pages using the 16G window at the start of address
 * space created by the bootloader. This will go away when this mapping is activated.
 */
void PTEHandler::initKernel() {

}




/**
 * Release all physical memory we allocated for page directories, tables, etc.
 *
 * @note You should not delete a page table that is currently mapped.
 */
PTEHandler::~PTEHandler() {
    // deallocate all physical pages we allocated 
    for(const auto physAddr : this->physToDealloc) {
        mem::PhysicalAllocator::free(physAddr);
    }
}

/**
 * Updates the processor's translation table register to use our translation tables.
 */
void PTEHandler::activate() {
    //*((volatile uint8_t *) 0x123456789ABC);

    panic("%s unimplemented", __PRETTY_FUNCTION__);

    // set the PDPT
    log("switching to PML4 $%016lx", this->pml4Phys);
    asm volatile("movq %0, %%cr3" :: "r" (this->pml4Phys));
}

/**
 * Read the CR3 reg and see if it contains the address of our PDPT.
 */
const bool PTEHandler::isActive() const {
    uint64_t pml4Addr;
    asm volatile("movq %%cr3, %0" : "=r" (pml4Addr));

    return (pml4Addr == this->pml4Phys);
}



/**
 * Maps a single 4K page.
 *
 * @return 0 on success, an error code otherwise
 */
int PTEHandler::mapPage(const uint64_t phys, const uintptr_t virt, const bool write,
        const bool execute, const bool global, const bool user, const bool noCache) {
    return 0;
}

/**
 * Unmaps a page. This does not release physical memory the page pointed to; only the memory of the
 * page table if all pages from it have been unmapped.
 *
 * @return 0 if the mapping was removed, 1 if no mapping was removed, or a negative error code.
 */
int PTEHandler::unmapPage(const uintptr_t virt) {
    return 0;
}

/**
 * Gets the physical address mapped to a given virtual address.
 *
 * @return 1 if no mapping exists (either page table not present or page not present), 0 if there
 * is one and its information is written to the provided variables.
 */
int PTEHandler::getMapping(const uintptr_t virt, uint64_t &phys, bool &write, bool &execute,
        bool &global, bool &user, bool &noCache) {
    return 0;
}
