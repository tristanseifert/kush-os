#include <ctype.h>
#include <wctype.h>
#include <stdio.h>
#include <locale.h>

/**
 * Returns the number of bytes in a multibyte string.
 */
size_t mbrlen(const char *s, size_t n, mbstate_t *ps) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}
