/*
 * Locale-aware string routines
 */
#ifndef _LOCALE_STRING_H
#define _LOCALE_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int     strcoll_l(const char *, const char *, locale_t);
size_t     strxfrm_l(char *, const char *, size_t, locale_t);
char    *strcasestr_l(const char *, const char *, locale_t);

#ifdef __cplusplus
}
#endif
#endif
