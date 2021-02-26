#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "file_private.h"

#include <printf.h>
#include <string.h>
#include <sys/syscalls.h>

/**
 * Printf callback that writes to the given stream.
 */
static void _WriteStreamCallback(char ch, void *arg) {
    FILE *stream = (FILE *) arg;
    fputc(ch, stream);
}

/**
 * Like vprintf(), but the output goes to the given file.
 */
int vfprintf(FILE *stream, const char *format, va_list arg) {
    if(!stream) return -1;

    return fctvprintf(_WriteStreamCallback, stream, format, arg);
}

/**
 * Like printf(), but the output goes to the given file stream. This just calls through to the
 * vfprintf() function.
 */
int fprintf(FILE *stream, const char *format, ...) {
    if(!stream) return -1;

    va_list va;
    va_start(va, format);
    int ret = vfprintf(stream, format, va);
    va_end(va);
    return ret;
}
