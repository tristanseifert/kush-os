#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"
#include "fd/map.h"

/**
 * Close the specified file descriptor.
 */
int fclose(FILE *stream) {
    int err = 0;

    // unregister the FD
#ifndef LIBC_NOTLS
    UnregisterFdStream(stream);
#endif

    // invoke the close handler
    if(stream->close) {
        err = stream->close(stream);
    }

    // XXX: is it a guarantee all file streams are allocated via malloc()?
    free(stream);
    return err;
}

/**
 * Closes all open descriptors.
 */
void fcloseall(void) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
}
