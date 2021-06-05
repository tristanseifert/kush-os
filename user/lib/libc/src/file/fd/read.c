#include "map.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>

/**
 * Reads from the file descriptor given by `filedes`
 */
ssize_t read(int filedes, void *buf, size_t nbyte) {
    // get the file descriptor
    stream_t *fp = ConvertFdToStream(filedes);
    if(!fp) {
        errno = EBADF;
        return -1 ;
    }

    // perform read
    if(fp->read) {
        return fp->read(fp, buf, nbyte);
    } else {
        errno = ENODEV;
        return -1;
    }
}

/**
 * Performs vectored IO to read from the file descriptor
 */
ssize_t readv(int filedes, const struct iovec *iov, int iovcnt) {
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
