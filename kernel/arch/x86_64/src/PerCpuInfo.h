#ifndef ARCH_x86_64_PERCPUINFO_H
#define ARCH_x86_64_PERCPUINFO_H

#define PROCI_OFF_SYSCALL_STACK         (24)
#define PROCI_OFF_PLATFORM              (56)

#ifndef ASM_FILE

#include <platform.h>

#if __has_include(<platform/CoreLocalInfo.h>)
#include <platform/CoreLocalInfo.h>
#endif

namespace sched {
class Scheduler;
struct Thread;
}

namespace arch {
class Idt;
class IrqRegistry;

/**
 * Per processor information structure
 */
struct ProcInfo {
    /// Self pointer
    ProcInfo *selfPtr = nullptr;
    /// processor ID (local APIC ID)
    uintptr_t procId = 0;

    /// Processor irql
    platform::Irql irql = platform::Irql::Passive;
    /// Stack pointer to use when entering syscalls
    void *syscallStack = nullptr;

    /// IDT pointer
    arch::Idt *idt = nullptr;
    /// interrupt registration (core specific)
    arch::IrqRegistry *irqRegistry = nullptr;

    /// core local scheduler
    sched::Scheduler *sched = nullptr;

    // platform specific info
#if PLATFORM_WANTS_CORE_LOCAL
    platform::CoreLocalInfo p;
#endif

    /// Initializes the self ptr
    ProcInfo() {
        this->selfPtr = this;
    }
};
static_assert(offsetof(ProcInfo, syscallStack) == PROCI_OFF_SYSCALL_STACK);
#if PLATFORM_WANTS_CORE_LOCAL
static_assert(offsetof(ProcInfo, p) == PROCI_OFF_PLATFORM);
#endif

/**
 * Handles per-processor information, which is accessible via the %gs segment in kernel space; like
 * userspace TLS, the first element is a pointer to the virtual address of the structure.
 */
class PerCpuInfo {
    public:
        /// Sets up the bootstrap processor per-CPU info struct
        static void BspInit();
        /// Set up an application processor's info struct
        static void ApInit();

        /// Returns the address of the current processor's info structure.
        static inline ProcInfo *get() {
            ProcInfo *ptr;
            asm volatile("movq %%gs:0, %0" : "=r"(ptr));
            return ptr;
        }

};

/**
 * Returns a reference to the architecture per-processor info structure.
 */
static inline ProcInfo *GetProcLocal() {
    return PerCpuInfo::get();
}
}

#endif
#endif
