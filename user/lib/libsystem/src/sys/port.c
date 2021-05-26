#include "syscall.h"
#include <sys/syscalls.h>

/**
 * Allocates a new port.
 */
int PortCreate(uintptr_t *outHandle) {
    intptr_t err = __do_syscall0(SYS_IPC_CREATE_PORT);

    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

/**
 * Destroys a previously allocated port handle.
 */
int PortDestroy(const uintptr_t portHandle) {
    return __do_syscall1(portHandle, SYS_IPC_DESTROY_PORT);
}

/**
 * Performs a message send to the given port.
 */
int PortSend(const uintptr_t portHandle, const void *message, const size_t messageLen) {
    return __do_syscall3(portHandle, (const uintptr_t) message, messageLen, SYS_IPC_MSGSEND);
}

/**
 * Attempts to receive a message from the given port.
 *
 * @param blockUs Microseconds to block waiting for a message; 0 means poll (i.e. do not block and
 *                fail immediately if no pending messages) while the value UINTPTR_MAX indicates
 *                we should block forever.
 */
int PortReceive(const uintptr_t portHandle, MessageHeader_t *buf,
        const size_t bufMaxLen, const uintptr_t blockUs) {
    return __do_syscall4(portHandle, (const uintptr_t) buf, bufMaxLen, blockUs, SYS_IPC_MSGRECV);
}

/**
 * Sets the queue depth (ceiling on the number of pending messages) for the given port.
 */
int PortSetQueueDepth(const uintptr_t portHandle, const uintptr_t queueDepth) {
    return __do_syscall2(portHandle, queueDepth, SYS_IPC_SET_PARAM_PORT);
}
