#ifndef _LIBC_LOCALE_H
#define _LIBC_LOCALE_H

#include <stdint.h>

/// Locale data structure
struct lconv {
    char    *decimal_point;
    char    *thousands_sep;
    char    *grouping;
    char    *int_curr_symbol;
    char    *currency_symbol;
    char    *mon_decimal_point;
    char    *mon_thousands_sep;
    char    *mon_grouping;
    char    *positive_sign;
    char    *negative_sign;
    char    int_frac_digits;
    char    frac_digits;
    char    p_cs_precedes;
    char    p_sep_by_space;
    char    n_cs_precedes;
    char    n_sep_by_space;
    char    p_sign_posn;
    char    n_sign_posn;
    char    int_p_cs_precedes;
    char    int_n_cs_precedes;
    char    int_p_sep_by_space;
    char    int_n_sep_by_space;
    char    int_p_sign_posn;
    char    int_n_sign_posn;
};

#define    LC_ALL                       0
#define    LC_COLLATE                   1
#define    LC_CTYPE                     2
#define    LC_MONETARY                  3
#define    LC_NUMERIC                   4
#define    LC_TIME                      5
#define    LC_MESSAGES                  6

#define    _LC_LAST                     7        /* marks end */


#ifdef __cplusplus
extern "C" {
#endif

struct lconv    *localeconv(void);
char            *setlocale(int, const char *);

#ifdef __cplusplus
}
#endif

/// below stuff is for XLocale support 
// forward declare the locale type
typedef struct _xlocale *locale_t;

/* Bit shifting order of LC_*_MASK should match XLC_* and LC_* order. */
#define LC_COLLATE_MASK                 (1<<0)
#define LC_CTYPE_MASK                   (1<<1)
#define LC_MONETARY_MASK                (1<<2)
#define LC_NUMERIC_MASK                 (1<<3)
#define LC_TIME_MASK                    (1<<4)
#define LC_MESSAGES_MASK                (1<<5)
#define LC_ALL_MASK                     (LC_COLLATE_MASK | LC_CTYPE_MASK | LC_MESSAGES_MASK | \
                                         LC_MONETARY_MASK | LC_NUMERIC_MASK | LC_TIME_MASK)
#define LC_VERSION_MASK                 (1<<6)
#define LC_GLOBAL_LOCALE                ((locale_t)-1)

#ifdef __cplusplus
extern "C" {
#endif

locale_t        duplocale(locale_t base);
void            freelocale(locale_t loc);
locale_t        newlocale(int mask, const char *locale, locale_t base);
const char      *querylocale(int mask, locale_t loc);
locale_t        uselocale(locale_t loc);

#ifdef __cplusplus
}
#endif

// include locale aware routines
#include <locale/_ctype.h>
#include <locale/_langinfo.h>
#include <locale/_stdlib.h>
#include <locale/_string.h>
#include <locale/_time.h>
#include <locale/_wchar.h>

#endif
