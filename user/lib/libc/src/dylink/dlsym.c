#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

/**
 * Looks up a symbol in the given library.
 *
 * This is a GNU extension, but libunwind requires it.
 */
void *dlsym(void *handle, const char *symbol) {
    fprintf(stderr, "%s unimplemented (symbol %s)\n", __PRETTY_FUNCTION__, symbol);

    errno = ENXIO;
    return NULL;
}

/**
 * Translates an address of a symbol to symbolic information. This includes its name, as well as
 * the dynamic object it is contained in.
 */
int dladdr(const void *addr, Dl_info *info) {
    fprintf(stderr, "%s unimplemented (address %p)\n", __PRETTY_FUNCTION__, addr);

    // TODO: implement
    errno = ENXIO;
    return 0;
}
