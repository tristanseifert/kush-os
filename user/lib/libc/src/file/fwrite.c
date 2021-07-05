#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "file_private.h"

/**
 * Writes a character to the file.
 */
int fputc(int ch, FILE *stream) {
    if(stream->putc) {
        return stream->putc(stream, ch);
    } 
    // XXX: should we keep this emulation?
    else if(stream->write) {
        return stream->write(stream, &ch, 1);
    }

    errno = ENODEV;
    return -1;
}

int putc(int ch, FILE *f) {
    return fputc(ch, f);
}

/**
 * Writes a string to the file.
 */
int fputs(const char *str, FILE *stream) {
    int i = 0, err;

    while(*str) {
        err = fputc(*str++, stream);
        if(err < 0) return err;
        else i++;
    }

    return i;
}

/**
 * Calls the device's write implementation.
 */
size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    if(stream->write) {
        return stream->write(stream, ptr, (size * nitems));
    }

    errno = ENODEV;
    return -1;
}
