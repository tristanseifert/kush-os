/*
 * Locale-aware character identification routines
 */
#ifndef _LOCALE_CTYPE_H
#define _LOCALE_CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

int islower_l(int c, locale_t locale);
int isupper_l(int c, locale_t locale);
int toupper_l(int c, locale_t locale);
int tolower_l(int c, locale_t locale);
int isdigit_l(int c, locale_t locale);
int isxdigit_l(int c, locale_t locale);
int isspace_l(int c, locale_t locale);

#ifdef __cplusplus
}
#endif
#endif
