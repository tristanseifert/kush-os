#ifndef ARCH_X86_VM_PTEHANDLER_H
#define ARCH_X86_VM_PTEHANDLER_H

#include <arch.h>
#include <bitflags.h>
#include <stdint.h>

#include <runtime/Vector.h>
#include <vm/IPTEHandler.h>

namespace arch::vm {
/**
 * Flags for a mapping on x86_64
 */
enum class PageFlags: uint64_t {
    None                                = 0,

    /// Mapping present
    Present                             = (1 << 0),
    /// Write allowed
    Writable                            = (1 << 1),
    /// User-mode access allowed
    UserAccess                          = (1 << 2),

    PWT                                 = (1 << 3),
    PCD                                 = (1 << 4),
    PAT                                 = (1 << 7),

    /// Whether this region has been accessed
    Accessed                            = (1 << 5),
    /// Whether we've written to this region before
    Dirty                               = (1 << 6),

    /// Mapping is global
    Global                              = (1 << 8),

    /**
     * Mapping is not executable. Note that this bit will NOT be present, even if the mapping was
     * originally created with the no-execute flag, if the underlying hardware doesn't support
     * no-execute. (It's a reserved bit that must be zero otherwise.)
     */
    NoExecute                           = (1ULL << 63),

    /// Mask of all bits corresponding to flags in a page table entry
    FlagsMask = (Present | Writable | UserAccess | PWT | PCD | PAT | Accessed | Dirty | Global |
            NoExecute),
};
ENUM_FLAGS_EX(PageFlags, uint64_t);

int ResolvePml4Virt(const uintptr_t pml4, const uintptr_t virt, uintptr_t &phys);

/**
 * Implements the x86_64-specific page table manipulation functions.
 *
 * We simply store the physical address of top-level paging structures and use the kernel's
 * physical identity mapping to access them directly.
 */
class PTEHandler: public ::vm::IPTEHandler {
    friend void ::arch_vm_available();

    public:
        /// Error and status codes for PTEHandler routines
        enum Status: int {
            /// No error, the operation completed successfully
            Success                     = 0,
            NoError                     = Success,
            /// The virtual address is non-canonical
            AddrNotCanonical            = -1,

            /// The virtual address is valid, but no mapping exists.
            NoMapping                   = 1,
            /// Virtual address is valid and was resolved to a physical address
            MappingResolved             = Success,

            /// Page was successfully unmapped
            PageUnmapped                = Success,
            /// No page at the given address to unmap
            NoPageToUnmap               = 1,

            /// Failed to allocate physical memory for a paging structure.
            NoMemory                    = -2,
            /// The virtual address specified already has a mapping.
            AlreadyMapped               = -3,
            /// A large page exists that already maps this 4K virtual address.
            AlreadyMappedLP             = -4,
        };

    public:
        PTEHandler() = delete;
        PTEHandler(::vm::IPTEHandler *parent);
        ~PTEHandler();

        void activate() override;
        const bool isActive() const override;

        int mapPage(const uint64_t phys, const uintptr_t virt, const bool write,
                const bool execute, const bool global, const bool user,
                const bool noCache) override;
        int unmapPage(const uintptr_t virt) override;

        int getMapping(const uintptr_t virt, uint64_t &phys, bool &write, bool &execute,
                bool &global, bool &user, bool &noCache) override;

        /// page tables can always be accessed through the ~ magic aperture ~
        bool supportsUnmappedModify(const uintptr_t virtAddr) override {
            return true;
        }

        /// The kernel map has been activated; so use the kernel physical memory aperture.
        static void InitialKernelMapLoad();

        /// Given a PML4 physical address, resolve a virtual address.
        static int Resolve(const uintptr_t pml4, const uintptr_t virt, uintptr_t &phys,
                PageFlags &flags);

    private:
        void initKernel();

        /// Returns the physical address of a zeroed page.
        static uintptr_t allocPage();

        /// Reads the nth index of the table with the given physical base address
        static uint64_t readTable(const uintptr_t tableBase, const size_t offset);
        /// Writes the nth index of the table with the given physical base address
        static void writeTable(const uintptr_t tableBase, const size_t offset, const uint64_t val);

        /// Translates the given table physical address into a virtual address
        static inline uint64_t *getTableVmAddr(const uintptr_t tableBase) {
            // XXX: should this panic instead?
            if(tableBase > (kPhysApertureSize * 1024 * 1024 * 1024)) return 0;

            if(gPhysApertureAvailable) [[likely]]
                return reinterpret_cast<uint64_t *>(tableBase + kPhysApertureBase);
            else [[unlikely]]
                return reinterpret_cast<uint64_t *>(tableBase);
        }

    private:
        /// First address of the kernel memory zone
        constexpr static const uintptr_t kKernelBoundary = 0x8000000000000000;

        /// Base address of the physical memory aperture
        constexpr static const uintptr_t kPhysApertureBase = 0xFFFF800000000000;
        /// Size of the physical aperture region, in GB (2048 is the max reserved)
        constexpr static const uintptr_t kPhysApertureSize = 512;
        static_assert(kPhysApertureSize <= 2048, "phys aperture max size (2TB) exceeded");

        /// When set, the high memory identity mapping is set up
        static bool gPhysApertureAvailable;
        /// Whether the physical aperture mappings are marked as global
        static bool gPhysApertureGlobal;

        static bool gLogAlloc;
        static bool gLogMapAdd, gLogMapRemove;

    private:
        // parent map
        PTEHandler *parent = nullptr;

        /// physical address of the PML4 table (root level)
        uintptr_t pml4Phys = 0;

        /// physical pages for paging structures of user mode addresses
        rt::Vector<uint64_t> physToDealloc;
};
}

#endif
