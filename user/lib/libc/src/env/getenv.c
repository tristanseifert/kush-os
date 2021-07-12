#include <stdlib.h>
#include <errno.h>
#include <string.h>

/**
 * Returns the value for an environment variable.
 */
char *getenv(const char *name) {
    // validate variable name
    if(!name || strlen(name) == 0 || strchr(name, '=')) {
        errno = EINVAL;
        return NULL;
    }

    // TODO: actually read environment variables
    return NULL;
}
