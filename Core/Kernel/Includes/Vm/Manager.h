#ifndef KERNEL_VM_MANAGER_H
#define KERNEL_VM_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#include <Bitflags.h>
#include <platform/Processor.h>

/// Virtual memory subsystem
namespace Kernel::Vm {
/**
 * @brief Virtual memory manager
 *
 * The virtual memory manager is primarily responsible for satisfying page faults.
 */
class Manager {
    public:
        static void Init();

        static void HandleFault(Platform::ProcessorState &state, const uintptr_t faultAddr);

    private:
        /// Shared (global) VM manager instance
        static Manager *gShared;
};

/**
 * @brief Virtual memory access mode
 *
 * You can OR most bits in this enumeration together to combine protection modes for a
 * particular page.
 */
enum class Mode: uintptr_t {
    /// No access is permitted
    None                                = 0,

    /// Kernel can read from this region
    KernelRead                          = (1 << 0),
    /// Kernel may write to this region
    KernelWrite                         = (1 << 1),
    /// Kernel can execute code out of this region
    KernelExec                          = (1 << 2),
    /// Kernel may read and write
    KernelRW                            = (KernelRead | KernelWrite),

    /// Userspace can read from this region
    UserRead                            = (1 << 8),
    /// Userspace may write to this region
    UserWrite                           = (1 << 9),
    /// Userspace may execute code out of this region
    UserExec                            = (1 << 10),
    /// Userspace may read and write
    UserRW                              = (UserRead | UserWrite),
    /// Mask for all user bits (any set = the mapping is user accessible)
    UserMask                            = (UserRead | UserWrite | UserExec),

    /// Mask indicating the read bits for kernel/userspace
    Read                                = (KernelRead | UserRead),
    /// Mask indicating the write bits for kernel/userspace
    Write                               = (KernelWrite | UserWrite),
    /// Mask indicating the exec bits for kernel/userspace
    Execute                             = (KernelExec | UserExec),
};
ENUM_FLAGS_EX(Mode, uintptr_t);

}

#endif
