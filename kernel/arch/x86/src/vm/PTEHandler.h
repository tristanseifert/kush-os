#ifndef ARCH_X86_VM_PTEHANDLER_H
#define ARCH_X86_VM_PTEHANDLER_H

#include <stdint.h>

#include <vm/IPTEHandler.h>

namespace arch { namespace vm {
/**
 * Implements the x86-specific page table manipulation functions.
 *
 * This will operate exclusively on PAE-enabled 32-bit page tables.
 */
class PTEHandler: public ::vm::IPTEHandler {
    public:
        PTEHandler(::vm::IPTEHandler *kernel = nullptr);
        ~PTEHandler();

        void activate() override;
        const bool isActive() const override;

        int mapPage(const uint64_t phys, const uintptr_t virt, const bool write,
                const bool execute, const bool global, const bool user) override;
        int unmapPage(const uintptr_t virt) override;

        int getMapping(const uintptr_t virt, uint64_t &phys, bool &write, bool &execute,
                bool &global, bool &user) override;

    private:
        void initKernel();
        void initCopyKernel(PTEHandler *);

        void setPageDirectory(const uint32_t virt, const uint64_t value);
        const uint64_t getPageDirectory(const uint32_t virt);
        void setPageTable(const uint32_t virt, const uint64_t value);
        const uint64_t getPageTable(const uint32_t virt);

    private:
        // physical address of the first level page directory pointer table
        uintptr_t pdptPhys = 0;
        // virtual address of the PDPT (if mapped)
        uint64_t *pdpt = nullptr;

        // physical addresses of the second level page directories
        uintptr_t pdtPhys[4] = {0, 0, 0, 0};
        // virtual addresses of the second level page directories
        uint64_t *pdt[4] = {(uint64_t *) 0xbf600000, (uint64_t *) 0xbf601000,
            (uint64_t *) 0xbf602000, (uint64_t *) 0xbf603000};
};
}}

#endif
