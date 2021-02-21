#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"

FILE *fopen(const char * restrict path, const char * restrict mode) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return NULL;
}

FILE *fdopen(int fildes, const char *mode) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return NULL;
}
