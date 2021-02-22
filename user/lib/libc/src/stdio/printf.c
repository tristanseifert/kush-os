#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

int printf(const char *format, ...) {
    int err;

    va_list va;
    va_start(va, format);

    err = vfprintf(stderr, format, va);

    va_end(va);

    return err;
}
