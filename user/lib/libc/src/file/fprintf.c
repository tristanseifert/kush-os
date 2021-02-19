#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Like vprintf(), but the output goes to the given file.
 */
int vfprintf(FILE *stream, const char *format, va_list arg) {
    return -1;
}

/**
 * Like printf(), but the output goes to the given file stream. This just calls through to the
 * vfprintf() function.
 */
int fprintf(FILE *stream, const char *format, ...) {
    va_list va;
    va_start(va, format);
    int ret = vfprintf(stream, format, va);
    va_end(va);
    return ret;
}
