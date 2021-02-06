#ifndef KERNEL_VM_IPTEHANDLER_H
#define KERNEL_VM_IPTEHANDLER_H

#include <stdint.h>

namespace vm {
/**
 * Abstract base class for a the architecture-specific page table handler.
 */
class IPTEHandler {
    public:
        IPTEHandler() = delete;
        IPTEHandler(IPTEHandler *parent) {};

        virtual void activate() = 0;
        virtual const bool isActive() const = 0;

        virtual int mapPage(const uint64_t phys, const uintptr_t virt, const bool write,
                const bool execute, const bool global, const bool user = false,
                const bool noCache = false) = 0;
        virtual int unmapPage(const uintptr_t virt) = 0;

        virtual int getMapping(const uintptr_t virt, uint64_t &phys, bool &write, bool &execute,
                bool &global, bool &user, bool &noCache) = 0;
};
}

#endif
