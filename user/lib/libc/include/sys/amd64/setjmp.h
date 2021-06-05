#ifndef LIBC_SYS_AMD64_SETJMP_H
#define LIBC_SYS_AMD64_SETJMP_H

/*
 * Offsets into a jump buffer to store the given registers.
 *
 * These are the callee saved registers in amd64 ABI.
 */
#define _JB_RBX                         0
#define _JB_RBP                         1
#define _JB_R12                         2
#define _JB_R13                         3
#define _JB_R14                         4
#define _JB_R15                         5
#define _JB_RSP                         6
#define _JB_PC                          7
#define _JB_SIGFLAG                     8
#define _JB_SIGMASK                     9
#define _JB_MXCSR                       10

/// size, in machine words, of a jmp_buf
#define _JBLEN 11

#endif
