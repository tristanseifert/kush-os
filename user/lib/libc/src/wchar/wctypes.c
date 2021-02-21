#include <ctype.h>
#include <wctype.h>
#include <locale.h>

// TODO: proper implementations

int iswspace(wint_t wc) {
    return (wc > 0xFF) ? 0 : isspace(wc);
}
int iswalpha(wint_t wc) {
    return (wc > 0xFF) ? 0 : isalpha(wc);
}
int iswblank(wint_t wc) {
    return (wc > 0xFF) ? 0 : isblank(wc);
}
int iswcntrl(wint_t wc) {
    return (wc > 0xFF) ? 0 : iscntrl(wc);
}
int iswlower(wint_t wc) {
    return (wc > 0xFF) ? 0 : islower(wc);
}
int iswupper(wint_t wc) {
    return (wc > 0xFF) ? 0 : isupper(wc);
}
int iswprint(wint_t wc) {
    return (wc > 0xFF) ? 0 : isprint(wc);
}

int iswdigit(wint_t wc) {
    return (wc > 0xFF) ? 0 : isdigit(wc);
}
int iswpunct(wint_t wc) {
    return (wc > 0xFF) ? 0 : ispunct(wc);
}
int iswxdigit(wint_t wc) {
    return (wc > 0xFF) ? 0 : isxdigit(wc);
}

wint_t towlower(wint_t wc) {
    return (wc > 0xFF) ? wc : tolower(wc);
}
wint_t towupper(wint_t wc) {
    return (wc > 0xFF) ? wc : toupper(wc);
}
