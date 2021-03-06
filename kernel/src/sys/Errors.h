/*
 * Error codes for system calls
 */
#ifndef KERNEL_SYS_ERRORS_H
#define KERNEL_SYS_ERRORS_H

namespace sys {
enum Errors: int {
    /// System call succeeded (not an error)
    Success                             = 0,
    /// Unspecified error
    GeneralError                        = -1,
    /// Invalid memory address/range provided
    InvalidPointer                      = -2,
    /// The provided handle was invalid
    InvalidHandle                       = -3,
    /// A provided argument was invalid
    InvalidArgument                     = -4,
    /// The syscall requested is unavailable
    InvalidSyscall                      = -5,
    /// A virtual address specified is invalid
    InvalidAddress                      = -6,
    /// Referenced memory is not mapped in the given process
    Unmapped                            = -7,
    /// The kernel is refusing to perform the given operation
    PermissionDenied                    = -8,
    /// The specified timeout has elapsed.
    Timeout                             = -9,
    /// Try the call again later (used for temporary failures)
    TryAgain                            = -10,
    /// The destination object is in the wrong state for the call
    InvalidState                        = -11,
    /// The call was aborted because it would result in a deadlock.
    DeadlockPrevented                   = -12,
    /// Required memory allocation failed
    NoMemory                            = -13,
    /// The buffer provided to the call is too small for the requested operation.
    BufferTooSmall                      = -14,
    /// The buffer provided to the call is too large for the requested operation.
    BufferTooLarge                      = -15,
};
}

#endif
