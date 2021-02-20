#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

/**
 * Same as sprintf, but we allocate a buffer to hold the output.
 */
int asprintf(char **strp, const char *fmt, ...) {
    return -1;
}

/**
 * Same as vsprintf, but we allocate a buffer to hold the output.
 */
int vasprintf(char **strp, const char *fmt, va_list ap) {
    return -1;
}
