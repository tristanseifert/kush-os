#include "syscall.h"
#include <sys/syscalls.h>

/**
 * Sets the given bits in the thread's notification bits.
 */
int NotificationSend(const uintptr_t threadHandle, const uintptr_t bits) {
    return __do_syscall2(threadHandle, bits, SYS_IPC_NOTE_SEND);
}

/**
 * Blocks the calling thread waiting to receive a notification, with the specified bit mask.
 *
 * @param blockUs Microseconds to block waiting for a message; 0 means poll (i.e. do not block and
 *                return the current notification flags immediately) while the value UINTPTR_MAX
 *                indicates we should block forever.
 *
 * @return Bitwise AND of mask and outstanding notifications.
 */
uintptr_t NotificationReceive(const uintptr_t mask, const uintptr_t timeout) {
    return __do_syscall2(mask, timeout, SYS_IPC_NOTE_RECEIVE);
}
