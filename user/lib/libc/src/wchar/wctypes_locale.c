#include <ctype.h>
#include <wctype.h>
#include <locale.h>

// TODO: update these with proper implementations

int iswalnum_l(wint_t wc, locale_t loc) {
    return iswalnum(wc);
}

int iswalpha_l(wint_t wc, locale_t loc) {
    return iswalpha(wc);
}

int iswascii_l(wint_t wc, locale_t loc) {
    return iswascii(wc);
}

int iswblank_l(wint_t wc, locale_t loc) {
    return iswblank(wc);
}

int iswcntrl_l(wint_t wc, locale_t loc) {
    return iswcntrl(wc);
}

int iswdigit_l(wint_t wc, locale_t loc) {
    return iswdigit(wc);
}

int iswgraph_l(wint_t wc, locale_t loc) {
    return iswgraph(wc);
}

int iswhexnumber_l(wint_t wc, locale_t loc) {
    return iswhexnumber(wc);
}

int iswideogram_l(wint_t wc, locale_t loc) {
    return iswideogram(wc);
}

int iswlower_l(wint_t wc, locale_t loc) {
    return iswlower(wc);
}

int iswnumber_l(wint_t wc, locale_t loc) {
    return iswnumber(wc);
}

int iswphonogram_l(wint_t wc, locale_t loc) {
    return iswphonogram(wc);
}

int iswprint_l(wint_t wc, locale_t loc) {
    return iswprint(wc);
}

int iswpunct_l(wint_t wc, locale_t loc) {
    return iswpunct(wc);
}

int iswrune_l(wint_t wc, locale_t loc) {
    return iswrune(wc);
}

int iswspace_l(wint_t wc, locale_t loc) {
    return iswspace(wc);
}

int iswspecial_l(wint_t wc, locale_t loc) {
    return iswspecial(wc);
}

int iswupper_l(wint_t wc, locale_t loc) {
    return iswupper(wc);
}

int iswxdigit_l(wint_t wc, locale_t loc) {
    return iswxdigit(wc);
}

wint_t towlower_l(wint_t wc, locale_t loc) {
    return towlower(wc);
}
wint_t towupper_l(wint_t wc, locale_t loc) {
    return towupper(wc);
}
