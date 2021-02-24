#include <sys/cdefs.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>

/**
 * Releases a locale.
 */
void freelocale(locale_t loc) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);    
}

/**
 * Sets a locale.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
char *setlocale(int category, const char *locale) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    
    // return current locale
    if(!locale) {
        return "";
    }
    
    // make the caller think they got one
    return locale;
}


#pragma clang diagnostic pop

/**
 * Sets the locale of the calling thread.
 */
locale_t uselocale(locale_t loc) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return loc;
}

/**
 * Creates a new locale.
 */
locale_t newlocale(int mask, const char * locale, locale_t base) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return NULL;
}


/**
 * Returns locale conventions for the given locale, or the current if NULL is specified.
 */
struct lconv *localeconv_l(locale_t loc) {
    fprintf(stderr, "%s unimplemented\n", __PRETTY_FUNCTION__);
    return NULL;
}

/**
 * Returns a struct of locale conventions for the current locale.
 */
struct lconv *localeconv(void) {
    return localeconv_l(NULL);
}
