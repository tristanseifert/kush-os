#include <ctype.h>

int iscntrl(const int c) {
    return (unsigned int)c < 0x20 || c == 0x7F; /* in the "C" locale */
}
