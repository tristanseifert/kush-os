#include <sys/cdefs.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include "locale_internal.h"

/**
 * Releases a locale.
 */
void freelocale(locale_t loc) {
    // release memory
    free(loc->name);
    free(loc);
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
locale_t newlocale(int mask, const char * name, locale_t base) {
    // create a new locale
    struct _xlocale *loc = calloc(1, sizeof(*loc));
    if(!loc) return NULL;

    if(name) {
        loc->name = strdup(name);
    } else {
        loc->name = strdup("C");
    }

    // TODO: copy the different masks
    return loc;
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
