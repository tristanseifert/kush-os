/*
 * Kush does NOT support signals!
 *
 * Instead, this header provides some of the structures and non-signal handling functions used
 * by other machine state-related (think setjmp/setcontext and friends) routines.
 */
#ifndef LIBC_SIGNAL_H
#define LIBC_SIGNAL_H

#include <_libc.h>

/**
 * Represents a stack to be used when executing user code.
 */
typedef struct stack {
    /// pointer to the base of the stack
    void *ss_sp;
    /// size of stack, in bytes
    long ss_size;
    /// flags
    int ss_flags;
} stack_t;

#endif
