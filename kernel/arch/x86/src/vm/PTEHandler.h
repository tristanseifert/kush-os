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
        PTEHandler();
        ~PTEHandler();

        void activate() override;

    private:
        // physical address of the first level page directory pointer table
        uintptr_t pdptPhys = 0;
        // virtual address of the PDPT (if mapped)
        uint64_t *pdpt = nullptr;

        // physical addresses of the second level page directories
        uintptr_t pdtPhys[4] = {0, 0, 0, 0};
        // virtual addresses of the second level page directories
        uint64_t *pdt[4] = {nullptr, nullptr, nullptr, nullptr};
};
}}

#endif
