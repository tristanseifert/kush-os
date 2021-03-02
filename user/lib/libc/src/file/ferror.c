#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"

/**
 * Test the end-of-file indicator of the given file.
 */
int feof(FILE *stream) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

/**
 * Tests the error indicator of the given file.
 */
int ferror(FILE *stream) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

