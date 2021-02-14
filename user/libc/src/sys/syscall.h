/*
 * Provides syscall stubs to get to the kernel.
 *
 * These functions are used internally by the library to implement other calls; you should not
 * call them directly.
 */
#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H

#include <_libc.h>
#include <stdint.h>

/// Makes a syscall with no arguments.
LIBC_INTERNAL int __do_syscall0(const uintptr_t number);
/// Makes a syscall with a single argument.
LIBC_INTERNAL int __do_syscall1(const uintptr_t number, const uintptr_t arg0);
/// Makes a syscall with two arguments.
LIBC_INTERNAL int __do_syscall2(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1);
/// Makes a syscall with three arguments.
LIBC_INTERNAL int __do_syscall3(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2);
/// Makes a syscall with four arguments.
LIBC_INTERNAL int __do_syscall4(const uintptr_t number, const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2, const uintptr_t arg3);

/*
 * Define syscall numbers.
 */
#define SYS_THREAD_GET_HANDLE           0x20
#define SYS_THREAD_YIELD                0x21
#define SYS_THREAD_SLEEP                0x22
#define SYS_THREAD_CREATE               0x23
#define SYS_THREAD_JOIN                 0x24
#define SYS_THREAD_DESTROY              0x25
#define SYS_THREAD_SET_STATE            0x26
#define SYS_THREAD_SET_PRIORITY         0x27
#define SYS_THREAD_SET_NOTEMASK         0x28
#define SYS_THREAD_RENAME               0x29

#define SYS_TASK_GET_HANDLE             0x30
#define SYS_TASK_CREATE                 0x31
#define SYS_TASK_TERMINATE              0x32
#define SYS_TASK_INIT                   0x33
#define SYS_TASK_RENAME                 0x34
#define SYS_TASK_WAIT                   0x35

#define SYS_TASK_DBG_OUT                0x36

#endif
