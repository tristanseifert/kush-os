/*
 * Locale-aware wide char routines
 */
#ifndef _LOCALE_WCHAR_H
#define _LOCALE_WCHAR_H

#ifdef __cplusplus
extern "C" {
#endif

int wcscoll_l(const wchar_t *ws1, const wchar_t *ws2, locale_t locale);
size_t wcsxfrm_l(wchar_t *ws1, const wchar_t *ws2, size_t n, locale_t locale);

int iswalpha_l(int wc, locale_t locale);
int iswdigit_l(int wc, locale_t locale);
int iswpunct_l(int wc, locale_t locale);
int iswxdigit_l(int wc, locale_t locale);
int iswblank_l(int wc, locale_t locale);
int iswspace_l(int wc, locale_t locale);
int iswlower_l(int wc, locale_t locale);
int iswupper_l(int wc, locale_t locale);
int iswspace_l(int wc, locale_t locale);
int iswcntrl_l(int wc, locale_t locale);
int iswprint_l(int wc, locale_t locale);

int towupper_l(int wc, locale_t locale);
int towlower_l(int wc, locale_t locale);

#ifdef __cplusplus
}
#endif
#endif
