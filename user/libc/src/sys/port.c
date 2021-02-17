#include "syscall.h"
#include <sys/syscalls.h>

/**
 * Allocates a new port.
 */
int PortCreate(uintptr_t *outHandle) {
    int err = __do_syscall0(SYS_IPC_CREATE_PORT);

    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

/**
 * Destroys a previously allocated port handle.
 */
int PortDestroy(const uintptr_t portHandle) {
    return __do_syscall1(SYS_IPC_DESTROY_PORT, portHandle);
}

