#include <_libc.h>

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <rpc/file.h>

/**
 * Gets information about a particular file.
 */
int stat(const char *filename, struct stat *buf) {
    errno = ENXIO;
    return -1;
}
