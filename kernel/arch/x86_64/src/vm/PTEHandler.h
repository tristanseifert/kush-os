#ifndef ARCH_X86_VM_PTEHANDLER_H
#define ARCH_X86_VM_PTEHANDLER_H

#include <arch.h>

#include <stdint.h>

#include <runtime/Vector.h>
#include <vm/IPTEHandler.h>

namespace arch::vm {
/**
 * Implements the x86_64-specific page table manipulation functions.
 *
 * We simply store the physical address of top-level paging structures and use the kernel's
 * physical identity mapping to access them directly.
 */
class PTEHandler: public ::vm::IPTEHandler {
    friend void ::arch_vm_available();

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

        static void InitialKernelMapLoad();

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
        /// Base address of the physical memory aperture
        constexpr static const uintptr_t kPhysApertureBase = 0xFFFF800000000000;
        /// Size of the physical aperture region, in GB (2048 is the max reserved)
        constexpr static const uintptr_t kPhysApertureSize = 512;

        /// When set, the high memory identity mapping is set up
        static bool gPhysApertureAvailable;
        /// Whether the physical aperture mappings are marked as global
        static bool gPhysApertureGlobal;

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
