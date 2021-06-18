#ifndef _LIBSYSTEM_SYSCALLS_IRQ_H
#define _LIBSYSTEM_SYSCALLS_IRQ_H

#include <stdint.h>

/// Return the vector number of the irq handler
#define SYS_IRQ_INFO_VECTOR             0x01

LIBSYSTEM_EXPORT int IrqHandlerInstall(const uintptr_t irqNum, const uintptr_t threadHandle,
        const uintptr_t bits, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int IrqHandlerRemove(const uintptr_t handle);
LIBSYSTEM_EXPORT int IrqHandlerUpdate(const uintptr_t handle, const uintptr_t threadHandle,
        const uintptr_t bits);
LIBSYSTEM_EXPORT int IrqHandlerGetInfo(const uintptr_t handle, const uintptr_t info);
LIBSYSTEM_EXPORT int IrqHandlerInstallLocal(const uintptr_t threadHandle, const uintptr_t bits);

#endif
