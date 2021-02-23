#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "file_private.h"
#include "rpc_file_streams.h"

/**
 * Table of file open handlers; multiple different backends can implement file-like interfaces. We
 * use this to simulate several pseudo-files that don't actually correspond to real files.
 */
static FILE* (*gOpenHandlers[])(const char *, const char *) = {
    __libc_rpc_file_open,
    NULL
};
#define kNumOpenHandlers                (1)

/**
 * Invoke all file open handlers that the library has in sequence to see if any can satisfy the
 * request.
 */
FILE *fopen(const char * restrict path, const char * restrict mode) {
    FILE *file = NULL;

    for(size_t i = 0; i < kNumOpenHandlers; i++) {
        file = (gOpenHandlers[i])(path, mode);
        if(file) return file;
    }

    // no handler could satisfy this request
    errno = ENOENT;
    return NULL;
}

FILE *fdopen(int fildes, const char *mode) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return NULL;
}
