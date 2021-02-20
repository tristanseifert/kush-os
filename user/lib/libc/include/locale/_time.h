/*
 * Locale-aware time output routines
 */
#ifndef _LOCALE_TIME_H
#define _LOCALE_TIME_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t strftime_l(char *s, size_t maxsize, const char *format, const struct tm *timeptr, locale_t locale);

#ifdef __cplusplus
}
#endif
#endif
