#include <ctype.h>
#include <locale.h>

int tolower(const int c) {
    return isupper(c) ? 'a' + (c - 'A') : c; /* in the "C" locale */
}

// TODO: proper implementation
int tolower_l(const int c, locale_t loc) {
    return tolower(c);
}
