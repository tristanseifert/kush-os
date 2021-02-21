#include <ctype.h>
#include <wctype.h>
#include <stdio.h>
#include <locale.h>


size_t wcsxfrm_l(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n, locale_t loc) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}

size_t wcsxfrm(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n) {
    return wcsxfrm_l(ws1, ws2, n, NULL);
}
