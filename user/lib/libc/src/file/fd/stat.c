#include "map.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

/**
 * Gets directory info for the given file.
 */
int fstat(int filedes, struct stat *buf) {
    // get the file descriptor
    stream_t *fp = ConvertFdToStream(filedes);
    if(!fp) {
        errno = EBADF;
        return -1;
    }

    // TODO: implement
    errno = EIO;
    return -1;
}
