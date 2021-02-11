#include "Handler.h"

#include "gdt.h"
#include <arch/x86_msr.h>
#include <log.h>
#include <new>

#include <sys/Syscall.h>

using namespace arch::syscall;

char gSharedBuf[sizeof(Handler)] __attribute__((aligned(64)));
Handler *Handler::gShared = nullptr;

extern "C" void arch_syscall_entry();

/**
 * Initializes the shared syscall handler.
 */
void Handler::init() {
    gShared = reinterpret_cast<Handler *>(&gSharedBuf);
    new(gShared) Handler();
}

/**
 * Initializes a syscall handler.
 *
 * We'll program the MSRs to make use of the SYSENTER/SYSEXIT instructions for fast syscalls. This
 * will land in the fast assembly stubs (in entry.S) so we can do really fast message passing, or
 * for a real syscall, fall back into the C++ "slow path."
 */
Handler::Handler() {
    // configure code segment and entry point
    x86_msr_write(kSysenterCsMsr, GDT_KERN_CODE_SEG, 0);
    x86_msr_write(kSysenterEipMsr, reinterpret_cast<uintptr_t>(&arch_syscall_entry), 0);
}



/**
 * Handles the given syscall.
 */
uintptr_t arch_syscall_handle(const uintptr_t number, const void *_args) {
    const auto args = reinterpret_cast<const sys::Syscall::Args *>(_args);

    // handle platform specific syscalls

    // handle HW specific syscalls

    // kernel syscalls
    return sys::Syscall::handle(args, number);
    return 0;
}

/**
 * Slow path for the msgsend-family of functions.
 */
uintptr_t arch_syscall_msgsend_slow(const uintptr_t type) {
    log("Slow path for msgsend (type %08x)", type);
    return -1;
}
