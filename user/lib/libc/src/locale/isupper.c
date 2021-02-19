#include <ctype.h>

int isupper(const int c) {
    return c >= 'A' && c <= 'Z'; /* in the "C" locale */
}
