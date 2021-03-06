#ifndef ARCH_X86_64_SYSCALL_HANDLER_H
#define ARCH_X86_64_SYSCALL_HANDLER_H

#ifndef ASM_FILE
#include <stdint.h>

#include <sched/Task.h>
#include <sched/Thread.h>
#include <arch.h>
#include <arch/x86_msr.h>

extern "C" uintptr_t arch_syscall_handle(const uintptr_t number, const void *args);
extern "C" uintptr_t arch_syscall_msgsend_slow(const uintptr_t eax);

namespace arch::syscall {
/**
 * Format of the time info page
 */
struct TimeInfo {
    /// Nanoseconds of kernel uptime
    uint64_t timeNsec;
    /// Time counter value when this was written
    uint64_t kernelTsc;
};

/**
 * Implements syscalls via the fast SYSENTER/SYSEXIT mechanism.
 */
class Handler {
    friend void ::arch_vm_available();
    friend void arch::Tick();

    private:
        /// SYSENTER code segment MSR (IA32_SYSENTER_CS)
        constexpr static const uintptr_t kSysenterCsMsr = 0x174;
        /// SYSENTER kernel stack MSR (IA32_SYSENTER_ESP)
        constexpr static const uintptr_t kSysenterEspMsr = 0x175;
        /// SYSENTER kernel entry point MSR (IA32_SYSENTER_EIP)
        constexpr static const uintptr_t kSysenterEipMsr = 0x176;

        /// Kernel VM address for the syscall stub page
        constexpr static const uintptr_t kStubKernelVmAddr = 0xFFFFFF0000010000;
        /// Userspace VM address for the syscall stub page
        constexpr static const uintptr_t kStubUserVmAddr = 0x00007FFF00000000;

        /// Kernel VM address for the system time page
        constexpr static const uintptr_t kTimeKernelVmAddr = 0xFFFFFF0000020000;
        /// Userspace VM address for the system time page
        constexpr static const uintptr_t kTimeUserVmAddr = 0x00007FFF00100000;

    public:
        /**
         * Handles a context switch to the given thread. We'll update the syscall entry stack to
         * the kernel stack associated with it. (This is okay, since syscalls can only be made
         * from user mode, when we have no use for the kernel stack.)
         */
        static inline void handleCtxSwitch(sched::Thread *thread) {
            x86_msr_write(kSysenterEspMsr, reinterpret_cast<uintptr_t>(thread->stack), 0);
        }

        /// Prepares the given task
        static inline void taskCreated(sched::Task *task) {
            gShared->mapSyscallStub(task);
            gShared->mapTimePage(task);
        }

    private:
        static void init();

        Handler();
        void mapSyscallStub(sched::Task *);
        void mapTimePage(sched::Task *);

        void updateTime();

    private:
        static Handler *gShared;

        // physical page holding the stub
        uint64_t stubPage = 0;
        // physical page holding the time information struct
        uint64_t timePage = 0;

        /// pointer to the in-memory time info struct
        TimeInfo *timeInfo = nullptr;
};
}
#endif

/// Last syscall number that uses the fast path (all calls incl+above this use slow path)
#define SYSCALL_FAST_MAX                0x10

#endif
