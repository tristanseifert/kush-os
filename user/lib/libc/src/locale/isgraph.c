#include <ctype.h>

int isgraph(const int c) {
    return c >= 0x21 && c <= 0x7E; /* in the "C" locale */
}
