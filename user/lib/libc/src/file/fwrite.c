#include <stdio.h>
#include <stdlib.h>
#include "file_private.h"

/**
 * Writes a character to the file.
 */
int fputc(int ch, FILE *stream) {
    return stream->putc(stream, ch);
}

size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}
