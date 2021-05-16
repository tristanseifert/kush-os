#include "../sys/syscall.h"
#include <sys/syscalls.h>
#include <sys/x86/syscalls.h>
#include <stdint.h>

/**
 * Updates the IO permission bitmap for the current task.
 */
int X86UpdateIopb(const void *bitmap, const uintptr_t bits, const uintptr_t offset) {
    return X86UpdateIopbFor(0, bitmap, bits, offset);
}

/**
 * Updates a subset of the IO permission bitmap for the specified task.
 */
int X86UpdateIopbFor(const uintptr_t taskHandle, const void *bitmap,
        const uintptr_t numBits, const uintptr_t offset) {
    // build the offset/length arg
    if(numBits > 65536 || (offset + numBits) > 65536) {
        return -1;
    }

    const uintptr_t off = (offset & 0xFFFF) | ((numBits << 16) & 0xFFFF0000);

    // perform call
    return __do_syscall3(taskHandle, (uintptr_t) bitmap, off, SYS_ARCH_X86_UPDATE_IOPB);
}

/**
 * Update the thread-local base of the current thread.
 */
int X86SetThreadLocalBase(const uintptr_t base) {
    return X86SetThreadLocalBaseFor(0, base);
}

/**
 * Update the thread-local base address for a particular thread. This is the base of the %gs
 * segment register.
 */
int X86SetThreadLocalBaseFor(const uintptr_t threadHandle, const uintptr_t base) {
    return __do_syscall2(threadHandle, base, SYS_ARCH_X86_SET_TLS_BASE);
}
