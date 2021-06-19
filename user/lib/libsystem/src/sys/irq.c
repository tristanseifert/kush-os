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

/**
 * Updates an IRQ handler's thread and notification bits.
 *
 * @note This will _replace_ notification bits. Therefore, calling with a value of 0 is invalid.
 */
int IrqHandlerUpdate(const uintptr_t irqHandle, const uintptr_t threadHandle, const uintptr_t bits) {
    return __do_syscall3(irqHandle, threadHandle, bits, SYS_ARCH_UPDATE_IRQ);
}

/**
 * Gets information about an IRQ handler.
 */
int IrqHandlerGetInfo(const uintptr_t irqHandle, const uintptr_t info) {
    return __do_syscall2(irqHandle, info, SYS_ARCH_IRQ_GETINFO);
}

/**
 * Creates a new IRQ handler that is bound to the current processor. This can be used to implement
 * driver specific IPIs or message-signaled hardware interrupts.
 *
 * The `IrqHandlerGetInfo` call can be used to discover the assigned vector number.
 */
int IrqHandlerInstallLocal(const uintptr_t threadHandle, const uintptr_t bits,
        uintptr_t *outHandle) {
    // validate args
    if(!outHandle) {
        return -1;
    }

    // do syscall
    intptr_t err = __do_syscall2(threadHandle, bits, SYS_ARCH_ALLOC_LOCAL);

    if(err > 0) {
        *outHandle = err;
        return 0;
    } else {
        return err;
    }
}

