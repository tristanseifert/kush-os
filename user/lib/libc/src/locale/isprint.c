#include <ctype.h>

int isprint(const int c) {
    return c >= 0x20 && c <= 0x7E; /* in the "C" locale */
}
