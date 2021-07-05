#include "map.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Closes the given file descriptor.
 *
 * This thunks through directly to `fclose` instead of directly invoking the close function, as
 * the internal structures relating to the file need to be deallocated as well. That will handle
 * deallocating the fd mapping as well.
 */
int close(int filedes) {
    // get the file descriptor
    stream_t *fp = ConvertFdToStream(filedes);
    if(!fp) {
        errno = EBADF;
        return -1;
    }

    return fclose(fp);
}
