#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

/**
 * Opens the dynamic library given by a certain filename, and adds it into the running process'
 * address space.
 *
 * Returned is an opaque handle, which can be passed to other dl* functions.
 */
void *dlopen(const char *filename, int flag) {
    fprintf(stderr, "%s unimplemented (path '%s' %u)\n", __PRETTY_FUNCTION__, filename, flag);

    errno = ENOENT;
    return NULL;
}

/**
 * Drops the reference count of a library previously loaded with dlopen(). When that count hits
 * zero, it's unloaded from the process' address space.
 */
int dlclose(void *handle) {
    fprintf(stderr, "%s unimplemented (handle %p)\n", __PRETTY_FUNCTION__, handle);

    errno = ENXIO;
    return -1;
}
