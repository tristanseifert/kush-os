#include "PTEHandler.h"

#include <vm/Map.h>
#include <mem/PhysicalAllocator.h>
#include <arch.h>
#include <log.h>
#include <string.h>

using namespace arch::vm;

/// Pointer to the kernel PTE
arch::vm::PTEHandler *gArchKernelPte = nullptr;
/*
 * Whether the high VM physical memory aperture (a 2TB region starting at 0xffff'8000'0000'0000)
 * has been set up (the kernel map has been activated at least once) or whether we're still in the
 * early boot-up phase where the low 16G or so of physical memory are identity mapped.
 */
bool PTEHandler::gPhysApertureAvailable = false;
bool PTEHandler::gPhysApertureGlobal = true;


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
    // store parent reference
    auto kernelPte = reinterpret_cast<PTEHandler *>(_parent);
    this->parent = kernelPte;

    // allocate root paging structure
    this->pml4Phys = allocPage();
    REQUIRE(this->pml4Phys, "failed to allocate %s", "PML4");

    // perform the rest of the initialization
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
    /*
     * Create the 2TB large physical region window at the bottom of the kernel address space. We
     * use 1GB pages to map this.
     *
     * This means we need four PDPTs, each of which has 512 entries, which then in turn map 512GB
     * of memory space.
     *
     * Note that these pages are all mapped as execute disable.
     */
    for(size_t i = 0; i < (kPhysApertureSize / 512); i++) {
        // allocate the PDPT
        auto pdpt = allocPage();
        REQUIRE(pdpt, "failed to allocate %s", "PDPT");

        const uint64_t physBase = (i * 0x8000000000);

        // fill in all 512 entries
        for(size_t j = 0; j < 512; j++) {
            uint64_t val = (physBase + (j * 0x40000000));

            val |= 0b10000011; // present, RW, supervisor, 1G
            if(arch_supports_nx()) {
                val |= (1ULL << 63); // execute disable
            }
            if(gPhysApertureGlobal) {
                val |= (1ULL << 8); // global
            }

            // write entry
            writeTable(pdpt, j, val);
        }

        // store PDPT in the PML4
        uint64_t pml4e = (pdpt & ~0xFFF);
        pml4e |= 0b00000011; // present, RW, supervisor

        if(arch_supports_nx()) {
            pml4e |= (1ULL << 63); // execute disable
        }

        writeTable(this->pml4Phys, 256 + i, pml4e);
    }
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
 * The kernel PTE has just been loaded for the first time.
 *
 * Switch from using the identity-mapping scheme (which the bootloader set up for us in the low
 * 16G or so of memory) to the physical aperture we set up earlier.
 */
void PTEHandler::InitialKernelMapLoad() {
    gPhysApertureAvailable = true;
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



/**
 * Reads the nth entry of the paging table, with the given physical base address.
 *
 * @param tableBase Physical address of the first entry of the table
 * @param offset Index into the table [0, 512)
 *
 * @return The 64-bit value in the table at the provided offset.
 */
uint64_t PTEHandler::readTable(const uintptr_t tableBase, const size_t offset) {
    REQUIRE(offset <= 511, "table offset out of range: %u", offset);

    auto ptr = getTableVmAddr(tableBase);
    return ptr[offset];
}

/**
 * Writes the nth entry of the specified paging table.
 *
 * @param tableBase Physical address of the first entry of the table
 * @param offset Index into the table [0, 512)
 * @param val Value to write at the index
 */
void PTEHandler::writeTable(const uintptr_t tableBase, const size_t offset, const uint64_t val) {
    REQUIRE(offset <= 511, "table offset out of range: %u", offset);

    auto ptr = getTableVmAddr(tableBase);
    ptr[offset] = val;
}

/**
 * Allocates a page of physical memory, to be used for paging structures. Ensure the memory is
 * zeroed (so that any attempts to dereference memory through it will fault)
 */
uintptr_t PTEHandler::allocPage() {
    auto page = mem::PhysicalAllocator::alloc();
    if(!page) return page;

    auto ptr = getTableVmAddr(page);
    memset(ptr, 0, 0x1000);

    return page;
}
