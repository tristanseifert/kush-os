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

    private:
        void initKernel();

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
