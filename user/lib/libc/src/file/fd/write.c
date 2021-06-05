#include "map.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>

/**
 * Writes to the file descriptor given by `filedes` the given byte range.
 */
ssize_t write(int filedes, const void *buf, size_t nbyte) {
    // get the file descriptor
    stream_t *fp = ConvertFdToStream(filedes);
    if(!fp) {
        errno = EBADF;
        return -1 ;
    }

    // perform write
    if(fp->write) {
        return fp->write(fp, buf, nbyte);
    } else {
        errno = ENODEV;
        return -1;
    }
}

/**
 * Performs vectored IO to the file descriptor given by `filedes`.
 */
ssize_t writev(int filedes, const struct iovec *iov, int iovcnt) {
    // get the file descriptor
    stream_t *fp = ConvertFdToStream(filedes);
    if(!fp) {
        errno = EBADF;
        return -1;
    }

    // TODO: implement
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return -1;
}
