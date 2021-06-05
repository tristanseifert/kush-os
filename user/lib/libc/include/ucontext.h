#ifndef LIBC_UCONTEXT_H
#define LIBC_UCONTEXT_H

#include <_libc.h>
#include <signal.h>

#if defined(__amd64__)
#include <sys/amd64/mcontext.h>
#endif

/**
 * Define an userspace context buffer
 */
typedef struct ucontext {
    /// context to resume when this one returns
    struct ucontext* uc_link;
    /// signal mask (required for compatibility but not used as no signals on kush)
    uintptr_t uc_sigmask;

    /// stack to use for this context
    stack_t uc_stack;
    /// machine context (registers, etc.)
    mcontext_t uc_mcontext;
} ucontext_t;



/// Initialize the specified user context
LIBC_EXPORT int getcontext(ucontext_t *ucp);
/// Restore the specified user context
LIBC_EXPORT int setcontext(const ucontext_t *ucp);

/// Initialize an user context to pass control to the given function
LIBC_EXPORT void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
/// Save the current context and swap to the specified one
LIBC_EXPORT int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);

#endif
