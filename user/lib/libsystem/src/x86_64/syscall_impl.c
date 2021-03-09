#include "sys/syscall.h"
#include <stdint.h>

/**
 * System calls are done using SYSCALL; it will write to RCX the return address, and to R11 the
 * flags value. You should set R9 to the stack pointer to restore when returning from a system
 * call.
 *
 * The syscall number is passed in %ax; The high 48 bits of %rax are reserved for syscall-specific
 * use. On return, %rax contains the return code.
 *
 * Syscalls may have up to 4 arguments, passed in the %rdi, %rsi, %rdx and %r8 registers,
 * respectively.
 */

