#include <ctype.h>

int isascii(const int c) {
    return c >= 0 && c <= 0x7F; /* 7-bit ASCII */
}
