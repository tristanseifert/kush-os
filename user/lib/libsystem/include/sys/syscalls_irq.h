#ifndef _LIBSYSTEM_SYSCALLS_IRQ_H
#define _LIBSYSTEM_SYSCALLS_IRQ_H

#include <stdint.h>

LIBSYSTEM_EXPORT int IrqHandlerInstall(const uintptr_t irqNum, const uintptr_t threadHandle,
        const uintptr_t bits, uintptr_t *outHandle);
LIBSYSTEM_EXPORT int IrqHandlerRemove(const uintptr_t handle);

#endif
