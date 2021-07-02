/*
 * Provides syscall stubs to get to the kernel.
 *
 * These functions are used internally by the library to implement other calls; you should not
 * call them directly.
 */
#ifndef LIBSYSTEM_SYSCALL_H
#define LIBSYSTEM_SYSCALL_H

#include <_libsystem.h>
#include <stdint.h>

/// Makes a syscall with no arguments.
LIBSYSTEM_INTERNAL intptr_t __do_syscall0(const uintptr_t number);
/// Makes a syscall with a single argument.
LIBSYSTEM_INTERNAL intptr_t __do_syscall1(const uintptr_t arg0, const uintptr_t number);
/// Makes a syscall with two arguments.
LIBSYSTEM_INTERNAL intptr_t __do_syscall2(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t number);
/// Makes a syscall with three arguments.
LIBSYSTEM_INTERNAL intptr_t __do_syscall3(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2, const uintptr_t number);
/// Makes a syscall with four arguments.
LIBSYSTEM_INTERNAL intptr_t __do_syscall4(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2, const uintptr_t arg3, const uintptr_t number);
/// Makes a syscall with five arguments.
LIBSYSTEM_INTERNAL intptr_t __do_syscall5(const uintptr_t arg0, const uintptr_t arg1, const uintptr_t arg2, const uintptr_t arg3, const uintptr_t arg4, const uintptr_t number);

/*
 * Define syscall numbers.
 */
#define SYS_IPC_MSGRECV                 0x00
#define SYS_IPC_MSGSEND                 0x01
#define SYS_IPC_SET_PARAM_PORT          0x02
#define SYS_IPC_CREATE_PORT             0x03
#define SYS_IPC_DESTROY_PORT            0x04
#define SYS_IPC_SHARE_VM                0x05

#define SYS_IPC_NOTE_RECEIVE            0x08
#define SYS_IPC_NOTE_SEND               0x09

#define SYS_VM_CREATE                   0x10
#define SYS_VM_CREATE_ANON              0x11
#define SYS_VM_DEALLOC                  0x12
#define SYS_VM_UPDATE_FLAGS             0x13
#define SYS_VM_RESIZE                   0x14
#define SYS_VM_MAP                      0x15
#define SYS_VM_MAP_EX                   0x16
#define SYS_VM_UNMAP                    0x17
#define SYS_VM_GET_INFO                 0x18
#define SYS_VM_GET_TASK_INFO            0x19
#define SYS_VM_ADDR_TO_HANDLE           0x1A
#define SYS_VM_VIRT_TO_PHYS             0x1B
#define SYS_VM_QUERY                    0x1C

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
#define SYS_THREAD_RESUME               0x2A

#define SYS_TASK_GET_HANDLE             0x30
#define SYS_TASK_CREATE                 0x31
#define SYS_TASK_TERMINATE              0x32
#define SYS_TASK_INIT                   0x33
#define SYS_TASK_RENAME                 0x34
#define SYS_TASK_WAIT                   0x35

#define SYS_TASK_DBG_OUT                0x36

#define SYS_ARCH_INSTALL_IRQ            0x38
#define SYS_ARCH_UNINSTALL_IRQ          0x39
#define SYS_ARCH_UPDATE_IRQ             0x3A
#define SYS_ARCH_IRQ_GETINFO            0x3B
#define SYS_ARCH_ALLOC_LOCAL            0x3C

#define SYS_MISC_GET_ENTROPY            0x40

// first architecture specific syscall number
#define SYS_ARCH                        0x48

#define SYS_ARCH_X86_UPDATE_IOPB        (SYS_ARCH + 0x00)
#define SYS_ARCH_X86_SET_TLS_BASE       (SYS_ARCH + 0x01)

#define SYS_ARCH_AMD64_SET_FGS_BASE     (SYS_ARCH + 0x00)
#define SYS_ARCH_AMD64_GET_LOADER_INFO  (SYS_ARCH + 0x01)
#define SYS_ARCH_AMD64_PORT_ALLOWLIST   (SYS_ARCH + 0x02)
#define SYS_ARCH_AMD64_PORT_ALLOWLIST_LOCK      (SYS_ARCH + 0x03)
#define SYS_ARCH_AMD64_PORT_READ        (SYS_ARCH + 0x04)
#define SYS_ARCH_AMD64_PORT_WRITE       (SYS_ARCH + 0x05)

#endif
