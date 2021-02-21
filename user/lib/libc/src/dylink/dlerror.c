#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

/**
 * Returns a human-readable description of the last error from the dynamic linker.
 *
 * After calling this function, the last error state is cleared.
 */
char *dlerror() {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return NULL;
}
