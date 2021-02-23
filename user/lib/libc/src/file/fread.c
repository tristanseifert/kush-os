#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "file_private.h"

size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    if(stream->read) {
        return stream->read(stream, ptr, (size * nitems));
    }

    errno = ENODEV;
    return -1;
}

