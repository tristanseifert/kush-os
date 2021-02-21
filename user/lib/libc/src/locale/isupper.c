#include <ctype.h>
#include <locale.h>

int isupper(const int c) {
    return c >= 'A' && c <= 'Z'; /* in the "C" locale */
}

// TODO: implement properly
int isupper_l(const int c, locale_t loc) {
    return isupper(c);
}
