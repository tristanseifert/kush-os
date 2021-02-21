#include <ctype.h>
#include <locale.h>

int toupper(const int c) {
    return islower(c) ? 'A' + (c - 'a') : c; /* in the "C" locale */
}

// TODO: proper implementation
int toupper_l(const int c, locale_t loc) {
    return toupper(c);
}

