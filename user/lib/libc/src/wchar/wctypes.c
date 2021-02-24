#include <ctype.h>
#include <wctype.h>
#include <locale.h>

// TODO: proper implementations

int iswspace(wint_t wc) {
    return (wc > 0xFF) ? 0 : isspace(wc);
}
int iswascii(wint_t wc) {
    return (wc > 0xFF) ? 0 : isascii(wc);
}
int iswhexnumber(wint_t wc) {
    return (wc > 0xFF) ? 0 : isxdigit(wc);
}
int iswalnum(wint_t wc) {
    return (wc > 0xFF) ? 0 : isalnum(wc);
}
int iswgraph(wint_t wc) {
    return (wc > 0xFF) ? 0 : isgraph(wc);
}
int iswspecial(wint_t wc) {
    return (wc > 0xFF) ? 0 : iscntrl(wc);
}
int iswnumber(wint_t wc) {
    return (wc > 0xFF) ? 0 : isdigit(wc);
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

int iswideogram(wint_t wc) {
    return 0;
}
int iswphonogram(wint_t wc) {
    return 0;
}
int iswrune(wint_t wc) {
    return 0;
}

wint_t towlower(wint_t wc) {
    return (wc > 0xFF) ? wc : tolower(wc);
}
wint_t towupper(wint_t wc) {
    return (wc > 0xFF) ? wc : toupper(wc);
}
