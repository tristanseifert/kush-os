#include <dlfcn.h>
#include <stddef.h>
#include <errno.h>

/**
 * Opens the dynamic library given by a certain filename, and adds it into the running process'
 * address space.
 *
 * Returned is an opaque handle, which can be passed to other dl* functions.
 */
void *dlopen(const char *filename, int flag) {
    errno = ENOENT;
    return NULL;
}

/**
 * Drops the reference count of a library previously loaded with dlopen(). When that count hits
 * zero, it's unloaded from the process' address space.
 */
int dlclose(void *handle) {
    errno = ENXIO;
    return -1;
}
