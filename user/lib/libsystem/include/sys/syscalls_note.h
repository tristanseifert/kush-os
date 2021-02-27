#ifndef _LIBSYSTEM_SYSCALLS_NOTIFICATIONS_H
#define _LIBSYSTEM_SYSCALLS_NOTIFICATIONS_H

#include <stdint.h>

LIBSYSTEM_EXPORT int NotificationSend(const uintptr_t threadHandle, const uintptr_t bits);
LIBSYSTEM_EXPORT uintptr_t NotificationReceive(const uintptr_t mask);

#endif
