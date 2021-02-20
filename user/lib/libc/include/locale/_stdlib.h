/*
 * Locale-aware string conversion routines
 */
#ifndef _LOCALE_STDLIB_H
#define _LOCALE_STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

long strtol_l(const char * nptr, char ** endptr, int base, locale_t loc);
long long strtoll_l(const char * nptr, char ** endptr, int base, locale_t loc);
unsigned long strtoul_l(const char * nptr, char ** endptr, int base, locale_t loc);
unsigned long long strtoull_l(const char * nptr, char ** endptr, int base, locale_t loc);
float strtof_l(const char *nptr, char **endptr, locale_t locale);
double strtod_l(const char *nptr, char **endptr, locale_t locale);
long double strtold_l(const char *nptr, char **endptr, locale_t locale);

#ifdef __cplusplus
}
#endif
#endif
