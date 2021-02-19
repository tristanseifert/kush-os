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

// VM syscalls
#include <sys/syscalls_vm.h>

// thread syscalls
#include <sys/syscalls_thread.h>

// task syscalls
#include <sys/syscalls_task.h>

#ifdef __cplusplus
}
#endif
#endif
