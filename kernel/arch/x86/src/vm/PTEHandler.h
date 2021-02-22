#ifndef ARCH_X86_VM_PTEHANDLER_H
#define ARCH_X86_VM_PTEHANDLER_H

#include <arch.h>

#include <stdint.h>

#include <runtime/Vector.h>
#include <vm/IPTEHandler.h>

namespace arch { namespace vm {
/**
 * Implements the x86-specific page table manipulation functions.
 *
 * This will operate exclusively on PAE-enabled 32-bit page tables.
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

        /// we must always be mapped to modify under kernel due to recursive mapping trick
        bool supportsUnmappedModify(const uintptr_t virtAddr) override {
            if(virtAddr >= 0xC0000000) return true;
            return false;
        }

    private:
        void initKernel();
        void initCopyKernel(PTEHandler *);

        /// maps the PDPTE of this PTE at a fixed virtual address
        void earlyMapPdpte();

        void setPageDirectory(const uint32_t virt, const uint64_t value);
        const uint64_t getPageDirectory(const uint32_t virt);
        void setPageTable(const uint32_t virt, const uint64_t value);
        const uint64_t getPageTable(const uint32_t virt);

        /**
         * Sets the "PDPTE Dirty" flag. When set, this will cause the PDPTE entries to get the
         * reserved bits masked off the next time the mapping is activated.
         *
         * This is required because accessing the PDPTE via recursive mapping will set in it the
         * accessed/dirty bits. This really shouldn't work (the formats aren't really orthogonal as
         * they are between PDTEs and PTEs) but it does so it's a nice little hack. The downside
         * here being that per the x86 Software Developer Manual (Section 4.4.1 Vol 3A) loading a
         * PDPTE with reserved bits set will cause a #GP fault.
         */
        inline void markPdpteDirty() {
            bool yes = true;
            __atomic_store(&this->pdpteDirty, &yes, __ATOMIC_RELAXED);
        }

    private:
        // parent map
        PTEHandler *parent = nullptr;

        // when set, this is a user mapping
        bool isUserMap = false;
        // physical address of the first level page directory pointer table
        uintptr_t pdptPhys = 0;
        // virtual address of the PDPT (if mapped)
        uint64_t *pdpt = nullptr;
        // when set, we've accessed the PDPTE via recursive mapping, and need to clear the bits
        bool pdpteDirty = false;

        // physical addresses of the second level page directories
        uintptr_t pdtPhys[4] = {0, 0, 0, 0};
        // virtual addresses of the second level page directories
        uint64_t *pdt[4] = {(uint64_t *) 0xbf600000, (uint64_t *) 0xbf601000,
            (uint64_t *) 0xbf602000, (uint64_t *) 0xbf603000};

        // list containing physical addresses of all private page tables (i.e. below 0xC0000000)
        rt::Vector<uint64_t> physToDealloc;
};
}}

#endif
