#include <errno.h>
#include <time.h>
#include <locale.h>
#include <stdio.h>

/**
 * Converts a given time value to string in the locale.
 */
size_t strftime_l(char *s, size_t maxsize, const char *format, const struct tm *timeptr,
                  locale_t loc) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return 0;
}

size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr) {
    return strftime_l(s, maxsize, format, timeptr, NULL);
}
