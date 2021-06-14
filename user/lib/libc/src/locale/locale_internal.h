#ifndef LIBC_LOCALE_LOCALE_INTERNAL
#define LIBC_LOCALE_LOCALE_INTERNAL

/**
 * Internal data type of the locale
 */
struct _xlocale {
    // locale name (duplicated)
    char *name;
};

#endif
