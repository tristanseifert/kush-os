#ifndef ARCH_x86_64_PERCPUINFO_H
#define ARCH_x86_64_PERCPUINFO_H

#define PROCI_OFF_SYSCALL_STACK         (16)

#ifndef ASM_FILE

#include <platform.h>

namespace arch {
/**
 * Per processor information structure
 */
struct ProcInfo {
    /// Self pointer
    ProcInfo *selfPtr = nullptr;

    /// Processor irql
    platform::Irql irql = platform::Irql::Passive;
    /// Stack pointer to use when entering syscalls
    void *syscallStack;

    /// Initializes the self ptr
    ProcInfo() {
        this->selfPtr = this;
    }
};
static_assert(offsetof(ProcInfo, syscallStack) == PROCI_OFF_SYSCALL_STACK);

/**
 * Handles per-processor information, which is accessible via the %gs segment in kernel space; like
 * userspace TLS, the first element is a pointer to the virtual address of the structure.
 */
class PerCpuInfo {
    public:
        /// Sets up the bootstrap processor per-CPU info struct
        static void BspInit();

        /// Returns the address of the current processor's info structure.
        static inline ProcInfo *get() {
            ProcInfo *ptr;
            asm volatile("movq %%gs:0, %0" : "=r"(ptr));
            return ptr;
        }

};
}

#endif
#endif
