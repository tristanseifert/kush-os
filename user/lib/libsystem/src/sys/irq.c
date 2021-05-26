#include "syscall.h"
#include <sys/syscalls.h>

/**
 * Installs an interrupt handler for a platform-specific interrupt number. When the interrupt is
 * fired, the provided thread has the given notification bits set.
 */
int IrqHandlerInstall(const uintptr_t irqNum, const uintptr_t threadHandle,
        const uintptr_t bits, uintptr_t *outHandle) {
    // validate args
    if(!outHandle) {
        return -1;
    }

    // do syscall
    intptr_t err = __do_syscall3(irqNum, threadHandle, bits, SYS_ARCH_INSTALL_IRQ);

    if(err > 0) {
        *outHandle = err;
        return 0;
    } else {
        return err;
    }
}

/**
 * Remove a previously installed interrupt handler.
 *
 * @note You can only remove interrupt handlers inside the calling task.
 */
int IrqHandlerRemove(const uintptr_t handle) {
    return __do_syscall1(handle, SYS_ARCH_UNINSTALL_IRQ);
}
