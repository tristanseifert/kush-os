#include <stdio.h>
#include <errno.h>

/**
 * Removes the file at the given path from the filesystem.
 */
int remove(const char *path) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);

    errno = EIO;
    return -1;
}
