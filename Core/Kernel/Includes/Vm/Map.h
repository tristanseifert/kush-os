#ifndef KERNEL_VM_MAP_H
#define KERNEL_VM_MAP_H

#include <stddef.h>
#include <stdint.h>

#include <Runtime/RefCountable.h>
#include <platform/PageTable.h>

namespace Kernel::Vm {
class MapEntry;

/**
 * @brief Virtual memory map
 *
 * These memory maps have a 1:1 correspondence to a set of hardware page tables. Each map consists
 * of multiple map entries.
 *
 * Internally, each map is backed by a platform-specific page table structure. This structure is
 * directly manipulated by VM objects (in order to add, modify or remove individual page mappings
 * to physical addresses) to change the page table. Outside of the VM object implementation, you
 * should always prefer to interact with maps through the higher level API.
 *
 * \section{Initialization}
 * Maps may be freely created as more unique memory spaces are required, with only a few caveats:
 *
 * 1. The first map that is created is automatically registered as the kernel's memory map. This
 *    means that any subsequently created maps will have this map as its "parent."
 */
class Map: public Runtime::RefCountable<Map> {
    friend class MapEntry;

    public:
        Map(Map *parent = nullptr);

        void activate();

        [[nodiscard]] int add(const uintptr_t base, MapEntry *entry);

    protected:
        ~Map() = delete;

    private:
        /// Map object for the kernel map
        static Map *gKernelMap;

        /**
         * Parent map
         *
         * The parent map is used for the kernel space mappings, if the platform has a concept of
         * separate kernel and userspace address spaces.
         */
        Map *parent{nullptr};

        /**
         * Platform page table instance
         *
         * This is the platform-specific wrapper to actually write out page tables, which can be
         * understood by the processor. Whenever a VM object wishes to change the page mappings, it
         * calls into methods on this object.
         */
        Platform::PageTable pt;
};
}

#endif
