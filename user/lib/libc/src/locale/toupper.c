#include <ctype.h>

int toupper(const int c) {
    return islower(c) ? 'A' + (c - 'a') : c; /* in the "C" locale */
}
