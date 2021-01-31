#ifndef KERNEL_VM_IPTEHANDLER_H
#define KERNEL_VM_IPTEHANDLER_H

namespace vm {
/**
 * Abstract base class for a the architecture-specific page table handler.
 */
class IPTEHandler {
    public:
        virtual void activate() = 0;
};
}

#endif
