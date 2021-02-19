#include <ctype.h>

int tolower(const int c) {
    return isupper(c) ? 'a' + (c - 'A') : c; /* in the "C" locale */
}
