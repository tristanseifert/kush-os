#include <ctype.h>
#include <wctype.h>
#include <stdio.h>
#include <locale.h>

/**
 * Converts a multibyte character to a wide char.
 */
int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return (size_t) -1;
}

/**
 * Convert a character to the corresponding wide char
 */
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return (size_t) -1;
}

/**
 * Stores a multibyte sequence required to represent the specified wide char.
 */
size_t wcrtomb(char *restrict s, wchar_t wc, mbstate_t *restrict ps) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return (size_t) -1;
}


/**
 * Converts a single byte character to the corresponding wide char.
 */
wint_t btowc(int c) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return c;
}

/**
 * Converts the corresponding wide char into a single byte character.
 */
int wctob(wint_t c) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    if(c < 0xFF) {
        return c;
    } else {
        return WEOF;
    }
}
