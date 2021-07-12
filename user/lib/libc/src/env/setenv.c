#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/**
 * Sets a given environment variable.
 */
int setenv(const char *name, const char *value, int overwrite) {
    // validate arguments
    if(!name || !strlen(name) || !value) {
        errno = EINVAL;
        return -1;
    }

    // TODO: implement
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    errno = EIO;
    return -1;
}

/**
 * Removes the specified environment key.
 */
int unsetenv(const char *name) {
    // TODO: implement
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    errno = EIO;
    return -1;
}
