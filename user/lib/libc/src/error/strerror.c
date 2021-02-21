#include <string.h>
#include <errno.h>
#include <stdio.h>

/**
 * Returns a descriptive string for the given error number.
 */
char * strerror(int errnum) {
    // TODO: implement
    return NULL;
}

/**
 * Copies out the descriptive string for the given error number.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wframe-address"

int strerror_r(int errnum, char *buf, size_t bufLen) {
    // TODO: implement
    snprintf(buf, bufLen, "TODO: strerror_r (%d) pc %p", errnum, __builtin_return_address(7));
    return 0;
}
