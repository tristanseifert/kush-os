#ifndef LIBSYSTEM_SYSCALLS_H
#define LIBSYSTEM_SYSCALLS_H

#include <_libsystem.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// IPC syscalls
#include <sys/syscalls_msg.h>
#include <sys/syscalls_note.h>
#include <sys/syscalls_irq.h>

// VM syscalls
#include <sys/syscalls_vm.h>

// thread syscalls
#include <sys/syscalls_thread.h>

// task syscalls
#include <sys/syscalls_task.h>

#include <sys/syscalls_misc.h>

#ifdef __cplusplus
}
#endif
#endif
