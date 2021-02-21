#include <ctype.h>
#include <wctype.h>
#include <stdio.h>
#include <locale.h>

/**
 * Convert a wide character string to a multibyte character string.
 */
size_t wcsnrtombs(char *dst, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}
size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}

/**
 * Converts a multibyte character string to a wide char string.
 */
size_t mbsnrtowcs(wchar_t *dst, const char **src, size_t nms, size_t len, mbstate_t *ps) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}

size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}
