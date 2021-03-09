#ifndef ARCH_X86_64_SYSCALL_HANDLER_H
#define ARCH_X86_64_SYSCALL_HANDLER_H

#ifndef ASM_FILE
#include <stdint.h>

#include <sched/Task.h>
#include <sched/Thread.h>
#include <arch.h>
#include <arch/x86_msr.h>
#include <arch/PerCpuInfo.h>

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
 * Implements syscalls via the fast SYSCALL/SYSRET mechanism.
 */
class Handler {
    friend void ::arch_vm_available();
    friend void arch::Tick();

    private:
        /// Kernel VM address for the system time page
        constexpr static const uintptr_t kTimeKernelVmAddr = 0xFFFFFF0000020000;
        /// Userspace VM address for the system time page
        constexpr static const uintptr_t kTimeUserVmAddr = 0x7FFF00100000;

    public:
        /**
         * When switching to a task, set its stack as the syscall stack in the per-CPU info
         * structure.
         */
        static inline void handleCtxSwitch(sched::Thread *thread) {
            PerCpuInfo::get()->syscallStack = thread->stack;
        }

        /// Prepares the given task
        static inline void taskCreated(sched::Task *task) {
            gShared->mapTimePage(task);
        }

    private:
        static void init();

        Handler();
        void mapTimePage(sched::Task *);

        void updateTime();

    private:
        static Handler *gShared;

        // physical page holding the time information struct
        uint64_t timePage = 0;
        /// pointer to the in-memory time info struct
        TimeInfo *timeInfo = nullptr;
};
}
#endif

#endif
