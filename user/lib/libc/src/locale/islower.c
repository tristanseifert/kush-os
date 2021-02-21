#include <ctype.h>
#include <locale.h>

int islower(const int c) {
    return c >= 'a' && c <= 'z'; /* in the "C" locale */
}

int islower_l(const int c, locale_t loc) {
    return islower(c);
}
