#include <stddef.h>
#include <stdio.h>
#include <dlfcn.h>
#include <link.h>

/**
 * Iterates the list of shared objects, invoking the user-specified callback for each of them.
 *
 * This is a Linux-ism, but libunwind seems to require it...
 */
int dl_iterate_phdr(int (*callback) (struct dl_phdr_info *info, const size_t size, void *data),
        void *data) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);

    // TODO: implement
    return 0;
}
