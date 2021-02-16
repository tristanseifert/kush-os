#include "../sys/syscall.h"
#include <sys/syscalls.h>
#include <sys/syscalls_arch_x86.h>
#include <stdint.h>

/**
 * Updates the IO permission bitmap for the current task.
 */
LIBC_EXPORT int X86UpdateIopb(const void *bitmap, const uintptr_t bits, const uintptr_t offset) {
    return X86UpdateIopbFor(0, bitmap, bits, offset);
}

/**
 * Updates a subset of the IO permission bitmap for the specified task.
 */
LIBC_EXPORT int X86UpdateIopbFor(const uintptr_t taskHandle, const void *bitmap,
        const uintptr_t numBits, const uintptr_t offset) {
    // build the offset/length arg
    if(numBits > 65536 || (offset + numBits) > 65536) {
        return -1;
    }

    const uintptr_t off = (offset & 0xFFFF) | ((numBits << 16) & 0xFFFF0000);

    // perform call
    return __do_syscall3(SYS_ARCH_X86_UPDATE_IOPB, taskHandle, (uintptr_t) bitmap, off);
}

