#ifndef ARCH_X86_SYSCALL_HANDLER_H
#define ARCH_X86_SYSCALL_HANDLER_H

#ifndef ASM_FILE
#include <stdint.h>

#include <sched/Thread.h>
#include <arch.h>
#include <arch/x86_msr.h>

extern "C" uintptr_t arch_syscall_handle(const uintptr_t eax);
extern "C" uintptr_t arch_syscall_msgsend_slow(const uintptr_t eax);

namespace arch { namespace syscall {
/**
 * Implements syscalls via the fast SYSENTER/SYSEXIT mechanism.
 */
class Handler {
    friend void ::arch_vm_available();

    private:
        /// SYSENTER code segment MSR (IA32_SYSENTER_CS)
        constexpr static const uintptr_t kSysenterCsMsr = 0x174;
        /// SYSENTER kernel stack MSR (IA32_SYSENTER_ESP)
        constexpr static const uintptr_t kSysenterEspMsr = 0x175;
        /// SYSENTER kernel entry point MSR (IA32_SYSENTER_EIP)
        constexpr static const uintptr_t kSysenterEipMsr = 0x176;

    public:
        /**
         * Handles a context switch to the given thread. We'll update the syscall entry stack to
         * the kernel stack associated with it. (This is okay, since syscalls can only be made
         * from user mode, when we have no use for the kernel stack.)
         */
        static inline void handleCtxSwitch(sched::Thread *thread) {
            x86_msr_write(kSysenterEspMsr, reinterpret_cast<uintptr_t>(thread->stack), 0);
        }

    private:
        static void init();

        Handler();

    private:
        static Handler *gShared;
};
}}
#endif

/// Last syscall number that uses the fast path (all calls incl+above this use slow path)
#define SYSCALL_FAST_MAX                0x10

#endif
