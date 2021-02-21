#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"

size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

