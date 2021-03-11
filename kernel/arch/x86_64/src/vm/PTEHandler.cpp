#include "PTEHandler.h"

#include <platform.h>
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

/// Whether allocations of paging structures are logged
bool PTEHandler::gLogAlloc = false;
/// Whether mappings are logged
bool PTEHandler::gLogMapAdd = false;
/// Whether removing of mappings are logged
bool PTEHandler::gLogMapRemove = false;

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
        this->initWithParent(kernelPte);
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
 * Initializes a page table that references the given parent table for all kernel-mode mappings.
 *
 * XXX: Currently, all PML4 entries above the kernel break are copied; any PML4 entries the kernel
 * adds later will NOT automatically be reflected here. This needs to be adressed.
 */
void PTEHandler::initWithParent(PTEHandler *parent) {
    // simply copy the last 256 entries of PML4
    for(size_t i = 255; i < 512; i++) {
        const auto in = readTable(parent->pml4Phys, i);
        writeTable(this->pml4Phys, i, in);
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

    platform::KernelMapEarlyInit();
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
 * All allocations above the user/supervisor split will NEVER be user readable; likewise, it is
 * not possible to create executable pages in kernel space after the kernel has booted.
 *
 * Any allocaed paging structures (PDPTs, PDTs, etc.) will be marked as writable, executable, and
 * depending on whether they're above the kernel boundary, supervisor-only.
 *
 * @return 0 on success, an error code otherwise
 */
int PTEHandler::mapPage(const uint64_t phys, const uintptr_t _virt, const bool write,
        const bool execute, const bool global, const bool user, const bool noCache) {
    uintptr_t ptAddr = 0;

    // ensure virtual address is canonical
    if(_virt > 0x00007FFFFFFFFFFF && _virt < 0xFFFF800000000000) {
        return Status::AddrNotCanonical;
    }

    // redirect requests for kernel mappings
    if(this->parent && _virt >= kKernelBoundary) {
        return this->parent->mapPage(phys, _virt, write, execute, global, user, noCache);
    }

    /*
     * Step through the PML4, PDPT, PDT, and eventually locate the address of the page table in
     * physical memory. If needed, we'll allocate pages for all of these. If there exists a mapping
     * for a larger page (for example, a 2M page in place of a page table pointer) we'll fail out.
     */
    const auto virt = _virt & 0xFFFFFFFFFFFF;
    if(gLogMapAdd) {
        log("Adding mapping: virt $%016llx -> phys $%016llx r%s%s %s%s", _virt, phys,
                write ? "w" : "", execute ? "x" : "", global ? "global " : "",
                user ? "user" : "");
    }

    // read the PML4 entry
    auto pml4e = readTable(this->pml4Phys, (virt >> 39) & 0x1FF);

    if(!(pml4e & (1 << 0))) { // allocate a PDPT
        auto page = allocPage();
        if(!page) {
            return Status::NoMemory;
        }

        if(_virt >= kKernelBoundary && gPhysApertureAvailable && !this->parent) {
            log("VM: added PDPT to kernel space, need to update children! (virt $%p)", _virt);
        }

        // update the PML4 to point at this page directory pointer table
        uint64_t entry = page;
        entry |= 0b11; // present, writable
        if(virt < kKernelBoundary) {
            entry |= (1 << 2);
        }

        writeTable(this->pml4Phys, (virt >> 39) & 0x1FF, entry);
        pml4e = entry;

        if(gLogAlloc) {
            log("Allocated %s: %016llx", "PDPT", entry);
        }
    }

    // read the PDPT entry
    const auto pdptAddr = (pml4e & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    auto pdpte = readTable(pdptAddr, (virt >> 30) & 0x1FF);

    if(!(pdpte & (1 << 0))) { // allocate a PDT
        auto page = allocPage();
        if(!page) {
            return Status::NoMemory;
        }

        // update the page directory pointer table to point at this page directory
        uint64_t entry = page;
        entry |= 0b11; // present, writable
        if(virt < kKernelBoundary) {
            entry |= (1 << 2);
        }

        writeTable(pdptAddr, (virt >> 30) & 0x1FF, entry);
        pdpte = entry;

        if(gLogAlloc) {
            log("Allocated %s: %016llx", "PDT", entry);
        }
    } else if(pdpte & (1 << 7)) { // present, 1G page
        return Status::AlreadyMappedLP;
    }

    // read the page directory entry to find page table
    const auto pdtAddr = (pdpte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    auto pdte = readTable(pdtAddr, (virt >> 21) & 0x1FF);

    if(!(pdte & (1 << 0))) { // allocate a page table
        auto page = allocPage();
        if(!page) {
            return Status::NoMemory;
        }

        // update page directory to point at this page table
        uint64_t entry = page;
        entry |= 0b11; // present, writable
        if(virt < kKernelBoundary) {
            entry |= (1 << 2);
        }

        writeTable(pdtAddr, (virt >> 21) & 0x1FF, entry);
        pdte = entry;

        if(gLogAlloc) {
            log("Allocated %s: %016llx", "PT", entry);
        }
    } else if(pdte & (1 << 7)) { // present, 2M page
        return Status::AlreadyMappedLP;
    }

    ptAddr = (pdte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);

    /*
     * Write the page table entry, into the page table whose physical address we've found above. We
     * need to build up the flags appropriately as well.
     */
    uint64_t pte = (phys & ~0xFFF) & ~static_cast<uint64_t>(PageFlags::FlagsMask);

    pte |= static_cast<uint64_t>(PageFlags::Present);

    if(write) {
        pte |= static_cast<uint64_t>(PageFlags::Writable);
    }
    if(global) {
        pte |= static_cast<uint64_t>(PageFlags::Global);
    }
    if(user) {
        pte |= static_cast<uint64_t>(PageFlags::UserAccess);
    }

    if(!execute && arch_supports_nx()) {
        pte |= static_cast<uint64_t>(PageFlags::NoExecute);
    }

    writeTable(ptAddr, (virt >> 12) & 0x1FF, pte);
    return Status::Success;
}

/**
 * Unmaps a page. This does not release physical memory the page pointed to; only the memory of the
 * page table if all pages from it have been unmapped.
 *
 * @return 0 if the mapping was removed, 1 if no mapping was removed, or a negative error code.
 */
int PTEHandler::unmapPage(const uintptr_t _virt) {
    const auto virt = _virt & 0xFFFFFFFFFFFF;
    if(gLogMapRemove) {
        log("Removing mapping: virt $%016llx", _virt);
    }

    // read the PML4 entry
    auto pml4e = readTable(this->pml4Phys, (virt >> 39) & 0x1FF);
    if(!(pml4e & (1 << 0))) {
        return Status::NoPageToUnmap;
    }

    // read the PDPT entry
    const auto pdptAddr = (pml4e & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    auto pdpte = readTable(pdptAddr, (virt >> 30) & 0x1FF);

    if(!(pdpte & (1 << 0))) {
        return Status::NoPageToUnmap;
    } else if(pdpte & (1 << 7)) { // present, 1G page
        return Status::AlreadyMappedLP;
    }

    // read the page directory entry
    const auto pdtAddr = (pdpte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    auto pdte = readTable(pdtAddr, (virt >> 21) & 0x1FF);

    if(!(pdte & (1 << 0))) {
        return Status::NoPageToUnmap;
    }  else if(pdte & (1 << 7)) { // present, 2M page
        return Status::AlreadyMappedLP;
    }

    // clear the page table entry
    const auto ptAddr = (pdte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);

    writeTable(ptAddr, (virt >> 12) & 0x1FF, 0);

    // TODO: release paging structures that became zeroed
    return Status::PageUnmapped;
}

/**
 * Gets the physical address mapped to a given virtual address.
 *
 * @return 1 if no mapping exists (either page table not present or page not present), 0 if there
 * is one and its information is written to the provided variables.
 */
int PTEHandler::getMapping(const uintptr_t virt, uint64_t &outPhys, bool &write, bool &execute,
        bool &global, bool &user, bool &noCache) {
    PageFlags flags = PageFlags::None;
    auto ret = Resolve(this->pml4Phys, virt, outPhys, flags);

    write = TestFlags(flags & PageFlags::Writable);
    global = TestFlags(flags & PageFlags::Global);
    user = TestFlags(flags & PageFlags::UserAccess);

    if(arch_supports_nx()) {
        execute = !TestFlags(flags & PageFlags::NoExecute);
    } else {
        execute = true;
    }

    if(ret == Status::MappingResolved) {
        return 0;
    } else if(ret == Status::NoMapping) {
        return 1;
    } else { // error case
        return ret;
    }
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

/**
 * Given the physical address of a PML4 table, attempt to resolve the given virtual address to a
 * physical address and associated flags.
 *
 * @return 0 if lookup was successful, negative error code otherwise.
 */
int PTEHandler::Resolve(const uintptr_t pml4, const uintptr_t _virt, uintptr_t &outPhys,
        PageFlags &outFlags) {
    PageFlags flags = PageFlags::None;

    // ensure address is canonical
    if(_virt > 0x00007FFFFFFFFFFF && _virt < 0xFFFF800000000000) {
        return Status::AddrNotCanonical;
    }

    // read the PML4 entry
    const auto virt = _virt & 0xFFFFFFFFFFFF;
    const auto pml4e = readTable(pml4, (virt >> 39) & 0x1FF);

    if(!(pml4e & (1 << 0))) {
        // PDPT not present
        // log("no %s: %016llx", "PDPT", pml4e);
        return Status::NoMapping;
    }

    // helper to gate off flags
    auto gater = [](PageFlags flags, const uint64_t pte, const bool access = false) -> PageFlags {
        if(TestFlags(static_cast<PageFlags>(pte) & PageFlags::NoExecute)) {
            flags |= PageFlags::NoExecute;
        }
        if(!TestFlags(static_cast<PageFlags>(pte) & PageFlags::Writable)) {
            flags &= ~PageFlags::Writable;
        }
        if(!TestFlags(static_cast<PageFlags>(pte) & PageFlags::UserAccess)) {
            flags &= ~PageFlags::UserAccess;
        }
        // set the access related flags if requested
        if(access) {
            if(TestFlags(static_cast<PageFlags>(pte) & PageFlags::Accessed)) {
                flags |= PageFlags::Accessed;
            }
            if(TestFlags(static_cast<PageFlags>(pte) & PageFlags::Dirty)) {
                flags |= PageFlags::Dirty;
            }
        }

        return flags;
    };

    // resolve PDPT and apply persistent flags
    if(TestFlags(static_cast<PageFlags>(pml4e) & PageFlags::NoExecute)) {
        flags |= PageFlags::NoExecute;
    }
    if(TestFlags(static_cast<PageFlags>(pml4e) & PageFlags::Writable)) {
        flags |= PageFlags::Writable;
    }
    if(TestFlags(static_cast<PageFlags>(pml4e) & PageFlags::UserAccess)) {
        flags |= PageFlags::UserAccess;
    }

    const auto pdptAddr = (pml4e & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    const auto pdpte = readTable(pdptAddr, (virt >> 30) & 0x1FF);
    if(!(pdpte & (1 << 0))) { // PDPT entry not present
        // log("no %s: %016llx", "PDT", pdpte);
        return Status::NoMapping;
    }
    else if(pdpte & (1 << 7)) { // 1G page
        outPhys = ((pdpte & ~0x3FFFFFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask)) + (virt & 0x3FFFFFFF);
        outFlags = gater(flags, pdpte, true);
        return Status::MappingResolved;
    }

    // resolve the PDT that this entry points to. also, take the rw. user and NX bits
    flags = gater(flags, pdpte);

    const auto pdtAddr = (pdpte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    const auto pdte = readTable(pdtAddr, (virt >> 21) & 0x1FF);
    if(!(pdte & (1 << 0))) { // PDT entry not present
        // log("no %s: %016llx", "PDT", pdte);
        return Status::NoMapping;
    }
    else if(pdte & (1 << 7)) { // 2M page
        outPhys = ((pdte & ~0x1FFFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask)) + (virt & 0x1FFFFF);
        outFlags = gater(flags, pdte, true);
        return Status::MappingResolved;
    }

    // resolve the page table the page directory points to
    flags = gater(flags, pdte);

    const auto ptAddr = (pdte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    const auto pte = readTable(ptAddr, (virt >> 12) & 0x1FF);
    if(!(pte & (1 << 0))) { // page table entry not present
        // log("no %s: %016llx", "PT", pte);
        return Status::NoMapping;
    }

    outPhys = ((pte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask)) + (virt & 0xFFF);
    outFlags = gater(flags, pte, true);
    return Status::MappingResolved;
}

int arch::vm::ResolvePml4Virt(const uintptr_t pml4, const uintptr_t virt, uintptr_t &phys) {
    PageFlags flags;
    return PTEHandler::Resolve(pml4, virt, phys, flags);
}

