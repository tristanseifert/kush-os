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
 * @return Bitwise AND of mask and outstanding notifications.
 */
uintptr_t NotificationReceive(const uintptr_t mask) {
    return __do_syscall1(mask, SYS_IPC_NOTE_RECEIVE);
}
