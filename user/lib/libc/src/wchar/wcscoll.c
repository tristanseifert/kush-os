#include <ctype.h>
#include <wctype.h>
#include <stdio.h>
#include <locale.h>


int wcscoll_l(const wchar_t *ws1, const wchar_t *ws2, locale_t loc) {
    fprintf(stderr, "%s unimplemented", __PRETTY_FUNCTION__);
    return -1;
}

int wcscoll(const wchar_t *ws1, const wchar_t *ws2) {
    return wcscoll_l(ws1, ws2, NULL);
}
