#ifndef LIBC_UIO_H
#define LIBC_UIO_H

#include <_libc.h>
#include <sys/cdefs.h>

/// Maximum of IO vectors per IO (XXX: this is arbitrarily set)
#define IOV_MAX         (100)

/**
 * Define a single IO buffer for use in vectored IO
 */
struct iovec {
    /// base address of buffer
    void	*iov_base;
    /// length of buffer (bytes)
    size_t	 iov_len;
};

/// Read from the given file descriptor with vectored IO
LIBC_EXPORT ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
/// Write to the given file descriptor with vectored IO
LIBC_EXPORT ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

#endif
