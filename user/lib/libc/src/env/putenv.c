#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/**
 * Writes a string of the form "name=value" to the environment.
 */
int putenv(char *string) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    errno = EIO;
    return -1;
}
