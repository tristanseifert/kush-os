/*
 * Locale-aware language database lookup routines
 */
#ifndef _LOCALE_LANGINFO_H
#define _LOCALE_LANGINFO_H

#include <nl_types.h>

#ifdef __cplusplus
extern "C" {
#endif

char    *nl_langinfo_l(nl_item, locale_t);

#ifdef __cplusplus
}
#endif
#endif
