#ifndef ARCH_X86_VM_PDPTPOOL_H
#define ARCH_X86_VM_PDPTPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void arch_vm_available();

namespace arch { namespace vm {
/**
 * Page directory pointer tables, or PDPTs, are the topmost level of the PAE paging structure. They
 * consist of four 8-byte entries, each mapping one gig of the virtual address space. Since these
 * structures are so small, we pool lots of them together in one physical page.
 *
 * The pool consists of a maximum of 16 pages, each containing some metadata (to indicate which
 * parts of it have been allocated or not) as well as storage for the actual PDPTs.
 *
 * PDPTs are always allocated below the 4G boundary, since the CR3 register is 32 bits only.
 *
 * @note This class is not usable until virtual memory has been set up.
 */
class PDPTPool {
    friend void ::arch_vm_available();

    public:
        /// Allocates memory for a new PDPT.
        static int alloc(uintptr_t &virtAddr, uintptr_t &physAddr) {
            if(!gShared) return -1;
            return gShared->getPdpt(virtAddr, physAddr);
        }
        /// Releases memory for the PDPT. Returns -1 if the physical address is not in the PDPT pool.
        static int free(const uint64_t physAddr) {
            if(!gShared->isPdptFromPool(physAddr)) {
                return -1;
            }

            gShared->freePdpt(physAddr);
            return 0;
        }

        static const bool isAvailable() {
            return gShared != nullptr;
        }

    private:
        /// max number of pages of PDPTs
        constexpr static const size_t kMaxPages = 16;

        /**
         * Structure stuffed inside a single 4K page.
         */
        struct Page {
            /// Physical address of the top of this page
            uintptr_t physAddr = 0;
            /// Bitmap indicating which directories are free (1)
            uint32_t freeMap[4];
            /// PDPT buffers; 32 bytes per
            uint64_t data[127][4] __attribute__((aligned(0x20)));

            /// Mark all PDPTs as available.
            Page(const uintptr_t _phys) : physAddr(_phys) {
                this->freeMap[0] = 0xFFFFFFFF;
                this->freeMap[1] = 0xFFFFFFFF;
                this->freeMap[2] = 0xFFFFFFFF;
                this->freeMap[3] = 0x7FFFFFFF;

                // ensure all PDPT entries are empty (e.g. not present!) by default
                memset(&this->data, 0, sizeof(this->data));
            }
            /// Are there free PDPTs in this page?
            const bool hasVacancies() const {
                return this->freeMap[0] || this->freeMap[1] || this->freeMap[2] ||
                       this->freeMap[3];
            }

            /// Allocates a PDPT from this page.
            bool alloc(uintptr_t &virt, uintptr_t &phys);
            /// Releases a PDPT based on its full physical address.
            void free(const uintptr_t phys);
        };
        static_assert((offsetof(Page, data) & 0x1F) == 0, "PDPT alignment violated");
        static_assert(sizeof(Page) <= 4096, "PDPT page struct too big");

    private:
        /// Sets up the PDPT pool.
        static void init();

    private:
        PDPTPool();
        ~PDPTPool();

        int getPdpt(uintptr_t &virt, uintptr_t &phys);
        void freePdpt(const uintptr_t phys);

        bool isPdptFromPool(const uintptr_t phys);

        bool allocPage();

    private:
        /// shared instance of the pool; set up once VM is available
        static PDPTPool *gShared;

    private:
        /// allocated pages, or null
        Page *pages[kMaxPages];
};

}}

#endif
