#include "map.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Performs a file seek.
 */
off_t lseek(int filedes, off_t offset, int whence) {
    // get the file descriptor
    stream_t *fp = ConvertFdToStream(filedes);
    if(!fp) {
        errno = EBADF;
        return -1;
    }

    return fseeko(fp, offset, whence);
}
