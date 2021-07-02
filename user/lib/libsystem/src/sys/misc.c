#include "syscall.h"
#include "helpers.h"
#include <sys/syscalls.h>

/**
 * Requests the specified number of bytes from the kernel's entropy buffer.
 */
int GetEntropy(void * _Nonnull outBuf, const size_t outBufSize) {
    return __do_syscall2((uintptr_t) outBuf, outBufSize, SYS_MISC_GET_ENTROPY);
}


