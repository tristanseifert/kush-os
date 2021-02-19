#include <dlfcn.h>
#include <stddef.h>

/**
 * Returns a human-readable description of the last error from the dynamic linker.
 *
 * After calling this function, the last error state is cleared.
 */
char *dlerror() {
    return NULL;
}
