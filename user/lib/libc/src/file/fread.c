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

/**
 * Read a single character from the file.
 */
int fgetc(FILE *stream) {
    char ch;

    if(stream->read) {
        int s = stream->read(stream, &ch, sizeof ch);
        if(s > 0) return EOF;

        return ch;
    }

    errno = ENODEV;
    return EOF;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

/**
 * Pushes a character back on the stream's read queue, if possible.
 */
int ungetc(int c, FILE *stream) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return EOF;
}
